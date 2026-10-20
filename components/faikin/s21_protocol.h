#pragma once

// Daikin S21 protocol layer.
//
// Pure C++ (no ESPHome dependency) so that it can be unit-tested on a host machine, see
// tests/test_s21_protocol.cpp. Everything here is derived from the community reverse engineering
// work (ESP32-Faikin / ESP32-Faikout and its S21 protocol wiki). Several fields are inferred and
// have not been confirmed on every unit; this is called out next to the relevant code.
//
// Wire format: 2400 baud, 8 data bits, even parity, 2 stop bits, master/slave (the indoor unit never
// transmits on its own).
//
//   request : STX c1 c2 [payload...] checksum ETX      -> unit answers ACK (or NAK if unsupported)
//   response: STX c1+1 c2 [payload...] checksum ETX    -> we answer ACK
//
// Commands starting with 'F' or 'R' are queries (answered by 'G' / 'S' frames), commands starting
// with 'D' are writes (answered by a bare ACK).

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace faikin {
namespace s21 {

static constexpr uint8_t STX = 0x02;
static constexpr uint8_t ETX = 0x03;
static constexpr uint8_t ENQ = 0x05;
static constexpr uint8_t ACK = 0x06;
static constexpr uint8_t NAK = 0x15;

static constexpr size_t MAX_PAYLOAD = 48;
static constexpr size_t MIN_FRAME = 5;  // STX c1 c2 checksum ETX
static constexpr size_t MAX_FRAME = MAX_PAYLOAD + MIN_FRAME;

// Target temperature is one character: '@' is 18.0 degC, +0.5 degC per step ('X' is 30.0 degC).
static constexpr uint8_t TEMP_BASE = '@';
static constexpr float TEMP_MIN = 18.0f;
static constexpr float TEMP_MAX = 30.0f;

// ---------------------------------------------------------------------------------------------
// Checksum and framing
// ---------------------------------------------------------------------------------------------

// A checksum that would collide with a control character is sent as value + 2. The community wiki
// lists 02, 03, 06 and 15 (hex). The original Faikin code only handled 03 -> 05.
inline uint8_t escape_checksum(uint8_t c) {
  switch (c) {
    case STX:
    case ETX:
    case ACK:
    case NAK:
      return static_cast<uint8_t>(c + 2);
    default:
      return c;
  }
}

// `len` is the total frame length. The checksum covers everything between STX and the checksum byte.
inline uint8_t checksum(const uint8_t *frame, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 1; i + 2 < len; i++)
    sum = static_cast<uint8_t>(sum + frame[i]);
  return escape_checksum(sum);
}

// Builds a request frame into `out` (at least MAX_FRAME bytes). Returns the frame length or 0 on error.
inline size_t build_frame(uint8_t c1, uint8_t c2, const uint8_t *payload, size_t payload_len, uint8_t *out) {
  if (payload_len > MAX_PAYLOAD)
    return 0;
  out[0] = STX;
  out[1] = c1;
  out[2] = c2;
  if (payload_len != 0)
    memcpy(out + 3, payload, payload_len);
  const size_t len = payload_len + MIN_FRAME;
  out[len - 2] = checksum(out, len);
  out[len - 1] = ETX;
  return len;
}

enum class ParseStatus : uint8_t { NEED_MORE, COMPLETE, TOO_LONG };
enum class FrameError : uint8_t { NONE, TOO_SHORT, BAD_CHECKSUM };

// Byte-by-byte frame receiver. Feed it every received byte; garbage before STX is ignored.
class FrameParser {
 public:
  void reset() {
    this->len_ = 0;
    this->active_ = false;
  }

  ParseStatus feed(uint8_t b) {
    if (!this->active_) {
      if (b == STX) {
        this->active_ = true;
        this->buf_[0] = b;
        this->len_ = 1;
      }
      return ParseStatus::NEED_MORE;
    }
    if (this->len_ >= MAX_FRAME) {
      this->reset();
      return ParseStatus::TOO_LONG;
    }
    this->buf_[this->len_++] = b;
    if (b == ETX) {
      this->active_ = false;
      return ParseStatus::COMPLETE;
    }
    return ParseStatus::NEED_MORE;
  }

  FrameError validate() const {
    if (this->len_ < MIN_FRAME)
      return FrameError::TOO_SHORT;
    if (checksum(this->buf_, this->len_) != this->buf_[this->len_ - 2])
      return FrameError::BAD_CHECKSUM;
    return FrameError::NONE;
  }

  const uint8_t *data() const { return this->buf_; }
  size_t length() const { return this->len_; }
  uint8_t cmd1() const { return this->len_ > 1 ? this->buf_[1] : 0; }
  uint8_t cmd2() const { return this->len_ > 2 ? this->buf_[2] : 0; }
  const uint8_t *payload() const { return this->buf_ + 3; }
  size_t payload_length() const { return this->len_ >= MIN_FRAME ? this->len_ - MIN_FRAME : 0; }

 protected:
  uint8_t buf_[MAX_FRAME]{};
  size_t len_{0};
  bool active_{false};
};

// ---------------------------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------------------------

enum class Mode : uint8_t { AUTO, HEAT, COOL, DRY, FAN };
enum class Fan : uint8_t { AUTO = 0, SPEED_1, SPEED_2, SPEED_3, SPEED_4, SPEED_5, QUIET };

inline uint8_t encode_mode(Mode m) {
  switch (m) {
    case Mode::HEAT:
      return '4';
    case Mode::COOL:
      return '3';
    case Mode::DRY:
      return '2';
    case Mode::FAN:
      return '6';
    default:
      return '0';
  }
}

// Wiki: 0, 1 and 7 are auto, 2 dry, 3 cool, 4 heat, 6 fan. 5 is not documented.
inline bool decode_mode(uint8_t v, Mode &m) {
  switch (v & 7) {
    case 0:
    case 1:
    case 7:
      m = Mode::AUTO;
      return true;
    case 2:
      m = Mode::DRY;
      return true;
    case 3:
      m = Mode::COOL;
      return true;
    case 4:
      m = Mode::HEAT;
      return true;
    case 6:
      m = Mode::FAN;
      return true;
    default:
      return false;
  }
}

inline uint8_t encode_fan(Fan f) {
  switch (f) {
    case Fan::AUTO:
      return 'A';
    case Fan::QUIET:
      return 'B';
    default:
      return static_cast<uint8_t>('3' + (static_cast<uint8_t>(f) - static_cast<uint8_t>(Fan::SPEED_1)));
  }
}

inline bool decode_fan(uint8_t v, Fan &f) {
  if (v == 'A') {
    f = Fan::AUTO;
    return true;
  }
  if (v == 'B') {
    f = Fan::QUIET;
    return true;
  }
  if (v >= '3' && v <= '7') {
    f = static_cast<Fan>(static_cast<uint8_t>(Fan::SPEED_1) + (v - '3'));
    return true;
  }
  return false;
}

inline uint8_t encode_target_temp(float t) {
  if (std::isnan(t))
    t = 22.0f;
  if (t < TEMP_MIN)
    t = TEMP_MIN;
  if (t > TEMP_MAX)
    t = TEMP_MAX;
  return static_cast<uint8_t>(TEMP_BASE + lroundf((t - TEMP_MIN) * 2.0f));
}

// Accepts a little more than the documented 18..30 range; anything else is treated as "not set".
inline float decode_target_temp(uint8_t v) {
  if (v < 0x38 || v > 0x60)
    return NAN;
  return TEMP_MIN + 0.5f * (static_cast<int>(v) - TEMP_BASE);
}

// Demand control ("power on demand"): limits the conditioning power to a percentage.
// Wire byte is 0x30 + (100 - percent), so 100 % (no limit) is '0'.
inline uint8_t encode_demand(int percent) {
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;
  return static_cast<uint8_t>(0x30 + (100 - percent));
}

inline bool decode_demand(uint8_t v, int &percent) {
  if (v < 0x30 || v > 0x30 + 100)
    return false;
  percent = 100 - (v - 0x30);
  return true;
}

// Three ASCII digits, units first: "522" decodes to 225.
inline bool decode_digits3(const uint8_t *p, int &out) {
  for (int i = 0; i < 3; i++)
    if (p[i] < '0' || p[i] > '9')
      return false;
  out = (p[0] - '0') + (p[1] - '0') * 10 + (p[2] - '0') * 100;
  return true;
}

// 4 bytes: three reversed digits (tenths of a degree) and a sign character ('-' is negative).
inline bool decode_temp_sensor(const uint8_t *p, float &out) {
  int v;
  if (!decode_digits3(p, v))
    return false;
  float t = v * 0.1f;
  if (p[3] == '-')
    t = -t;
  if (t <= -50.0f || t >= 100.0f)
    return false;
  out = t;
  return true;
}

// F9 style: 0x80 + 2 * degC.
inline bool decode_half_degrees(uint8_t b, float &out) {
  const float t = (static_cast<int>(b) - 0x80) / 2.0f;
  if (t <= -50.0f || t >= 100.0f)
    return false;
  out = t;
  return true;
}

// ---------------------------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------------------------

// Groups of values that are read together and written together.
enum Group : uint8_t {
  GROUP_D1 = 1,       // power, mode, target temperature, fan          (F1 / D1)
  GROUP_D5 = 2,       // swing                                          (F5 / D5)
  GROUP_D6 = 4,       // powerful, comfort, quiet, streamer, sensor LED (F6 / D6)
  GROUP_D7 = 8,       // econo, demand                                  (F7 / D7)
  GROUP_SENSORS = 16  // read only measurements
};

struct State {
  bool power{false};
  Mode mode{Mode::AUTO};
  float target_temp{NAN};
  Fan fan{Fan::AUTO};

  bool swing_v{false};
  bool swing_h{false};

  bool powerful{false};
  bool comfort{false};
  bool quiet{false};  // outdoor unit quiet mode
  bool streamer{false};
  bool sensor_led{false};

  bool econo{false};
  int demand{-1};  // percent, -1 when unknown

  float home{NAN};
  float outside{NAN};
  float liquid{NAN};
  int fan_rpm{-1};
  int comp{-1};

  // Which groups have been read at least once.
  uint8_t known{0};
};

inline bool feq(float a, float b) { return (std::isnan(a) && std::isnan(b)) || a == b; }

inline bool state_equal(const State &a, const State &b) {
  return a.power == b.power && a.mode == b.mode && feq(a.target_temp, b.target_temp) && a.fan == b.fan &&
         a.swing_v == b.swing_v && a.swing_h == b.swing_h && a.powerful == b.powerful && a.comfort == b.comfort &&
         a.quiet == b.quiet && a.streamer == b.streamer && a.sensor_led == b.sensor_led && a.econo == b.econo &&
         a.demand == b.demand && feq(a.home, b.home) && feq(a.outside, b.outside) && feq(a.liquid, b.liquid) &&
         a.fan_rpm == b.fan_rpm && a.comp == b.comp && a.known == b.known;
}

// Copies the fields of the given groups from `src` to `dst`.
inline void copy_groups(State &dst, const State &src, uint8_t groups) {
  if (groups & GROUP_D1) {
    dst.power = src.power;
    dst.mode = src.mode;
    dst.target_temp = src.target_temp;
    dst.fan = src.fan;
  }
  if (groups & GROUP_D5) {
    dst.swing_v = src.swing_v;
    dst.swing_h = src.swing_h;
  }
  if (groups & GROUP_D6) {
    dst.powerful = src.powerful;
    dst.comfort = src.comfort;
    dst.quiet = src.quiet;
    dst.streamer = src.streamer;
    dst.sensor_led = src.sensor_led;
  }
  if (groups & GROUP_D7) {
    dst.econo = src.econo;
    dst.demand = src.demand;
  }
  dst.known |= (src.known & groups);
}

// ---------------------------------------------------------------------------------------------
// Response decoding
// ---------------------------------------------------------------------------------------------

// Applies a response frame to `st`. Returns the set of groups that were updated (0 if the frame was
// not understood or too short).
inline uint8_t apply_response(State &st, uint8_t c1, uint8_t c2, const uint8_t *p, size_t n) {
  if (c1 == 'G') {
    switch (c2) {
      case '1': {  // basic status
        if (n < 4)
          return 0;
        st.power = (p[0] == '1');
        Mode m;
        if (decode_mode(p[1], m))
          st.mode = m;
        // The target temperature is only reported in heat, cool and auto modes.
        if (st.mode == Mode::AUTO || st.mode == Mode::HEAT || st.mode == Mode::COOL) {
          const float t = decode_target_temp(p[2]);
          if (!std::isnan(t))
            st.target_temp = t;
        }
        Fan f;
        if (p[3] == 'A') {
          // Some units report "auto" while the fan is in quiet mode. Keep quiet unless the fan is
          // clearly running fast (heuristic from the original Faikin code).
          const bool fast = st.fan_rpm >= 750;
          if (!(st.fan == Fan::QUIET && st.power && !fast))
            st.fan = Fan::AUTO;
        } else if (decode_fan(p[3], f)) {
          st.fan = f;
        }
        st.known |= GROUP_D1;
        return GROUP_D1;
      }
      case '5':  // swing
        if (n < 1)
          return 0;
        st.swing_v = (p[0] & 1) != 0;
        st.swing_h = (p[0] & 2) != 0;
        st.known |= GROUP_D5;
        return GROUP_D5;
      case '6':  // "powerful" and other special modes
        if (n < 4)
          return 0;
        st.powerful = (p[0] & 0x02) != 0;
        st.comfort = (p[0] & 0x40) != 0;
        st.quiet = (p[0] & 0x80) != 0;
        st.streamer = (p[1] & 0x80) != 0;
        st.sensor_led = (p[3] & 0x08) != 0;
        st.known |= GROUP_D6;
        return GROUP_D6;
      case '7': {  // econo and demand control
        if (n < 2)
          return 0;
        st.econo = (p[1] & 0x02) != 0;
        int pct;
        st.demand = decode_demand(p[0], pct) ? pct : -1;
        st.known |= GROUP_D7;
        return GROUP_D7;
      }
      case '9': {  // coarse room / outside temperature (fallback when RH / Ra are unavailable)
        if (n < 2)
          return 0;
        float t;
        if (decode_half_degrees(p[0], t))
          st.home = t;
        if (decode_half_degrees(p[1], t))
          st.outside = t;
        return GROUP_SENSORS;
      }
      default:
        return 0;
    }
  }
  if (c1 == 'S') {
    switch (c2) {
      case 'L':    // indoor fan speed, tens of rpm
      case 'd': {  // compressor
        if (n < 3)
          return 0;
        int v;
        if (!decode_digits3(p, v))
          return 0;
        if (c2 == 'L')
          st.fan_rpm = v * 10;
        else
          st.comp = v;
        return GROUP_SENSORS;
      }
      case 'H':    // room temperature
      case 'a':    // outside temperature
      case 'I': {  // liquid / coil temperature (semantics not fully confirmed)
        if (n < 4)
          return 0;
        float t;
        if (!decode_temp_sensor(p, t))
          return 0;
        if (c2 == 'H')
          st.home = t;
        else if (c2 == 'a')
          st.outside = t;
        else
          st.liquid = t;
        return GROUP_SENSORS;
      }
      default:
        return 0;
    }
  }
  return 0;
}

// ---------------------------------------------------------------------------------------------
// Write payloads (all are 4 bytes)
// ---------------------------------------------------------------------------------------------

inline void build_d1(const State &d, uint8_t out[4]) {
  out[0] = d.power ? '1' : '0';
  out[1] = encode_mode(d.mode);
  // No target temperature in dry and fan modes.
  out[2] = (d.mode == Mode::AUTO || d.mode == Mode::HEAT || d.mode == Mode::COOL) ? encode_target_temp(d.target_temp)
                                                                                    : TEMP_BASE;
  out[3] = encode_fan(d.fan);
}

inline void build_d5(const State &d, uint8_t out[4]) {
  const bool any = d.swing_h || d.swing_v;
  out[0] = static_cast<uint8_t>('0' + (d.swing_h ? 2 : 0) + (d.swing_v ? 1 : 0) + (d.swing_h && d.swing_v ? 4 : 0));
  out[1] = any ? '?' : '0';
  out[2] = '0';
  out[3] = '0';
}

inline void build_d6(const State &d, uint8_t out[4]) {
  out[0] = static_cast<uint8_t>('0' + (d.powerful ? 0x02 : 0) + (d.comfort ? 0x40 : 0) + (d.quiet ? 0x80 : 0));
  out[1] = static_cast<uint8_t>('0' + (d.streamer ? 0x80 : 0));
  out[2] = '0';
  out[3] = static_cast<uint8_t>('0' + (d.sensor_led ? 0x08 : 0));
}

// Demand and econo share one frame: writing econo alone would reset the demand limit to 100 %.
inline void build_d7(const State &d, uint8_t out[4]) {
  out[0] = encode_demand(d.demand < 0 ? 100 : d.demand);
  out[1] = static_cast<uint8_t>('0' + (d.econo ? 0x02 : 0));
  out[2] = '0';
  out[3] = '0';
}

// ---------------------------------------------------------------------------------------------
// Probe helpers (identification strings are reversed ASCII, zero padded)
// ---------------------------------------------------------------------------------------------

// Reverses `n` bytes into `out` (size n + 1) and replaces non printable characters with '.'.
// Padding is not trimmed: the caller sees the full string.
inline void reverse_ascii(const uint8_t *p, size_t n, char *out) {
  for (size_t i = 0; i < n; i++) {
    const uint8_t c = p[n - 1 - i];
    out[i] = (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : '.';
  }
  out[n] = '\0';
}

// FY00 holds the protocol version as reversed ASCII, e.g. "0230" -> "0320" -> 3.20. The payload may
// or may not echo the selector, so only the last four bytes are used. Returns false if the bytes
// are not four digits.
inline bool format_protocol_version(const uint8_t *p, size_t n, char *out, size_t cap) {
  if (n < 4 || cap < 8)
    return false;
  const uint8_t *q = p + (n - 4);
  char r[5];
  reverse_ascii(q, 4, r);
  for (int i = 0; i < 4; i++)
    if (r[i] < '0' || r[i] > '9')
      return false;
  const int major = (r[0] - '0') * 10 + (r[1] - '0');
  const int minor = (r[2] - '0') * 10 + (r[3] - '0');
  snprintf(out, cap, "%d.%02d", major, minor);
  return true;
}

}  // namespace s21
}  // namespace faikin
}  // namespace esphome
