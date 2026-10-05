#pragma once

// Pure protocol helpers for VE.Can (Victron's NMEA 2000 flavour).
// No ESPHome dependency on purpose: this header is unit-tested on the host with plain g++.
//
// References (public Victron documents):
//  - "VE.Can registers - public" (Victron Registers in NMEA 2000, v23)
//  - "Data communication with Victron Energy products" (whitepaper, PGN tables, FAQ)

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace esphome {
namespace vecan {

// ---------------------------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------------------------
static const uint16_t VICTRON_MANUFACTURER_CODE = 358;  // 0x166
static const uint8_t VREG_HDR0 = 0x66;                  // manufacturer code 358 + reserved + marine industry group,
static const uint8_t VREG_HDR1 = 0x99;                  // little endian: 0x9966

static const uint8_t ADDR_NULL = 0xFE;    // "cannot claim" address
static const uint8_t ADDR_GLOBAL = 0xFF;  // broadcast

static const uint32_t PGN_ISO_REQUEST = 0xEA00;
static const uint32_t PGN_ISO_ADDRESS_CLAIM = 0xEE00;
static const uint32_t PGN_VREG_SF = 0xEF00;   // proprietary, single frame, addressable
static const uint32_t PGN_VREG_FP = 0x1EF00;  // proprietary, fast packet, addressable
static const uint32_t PGN_BINARY_STATUS = 127501;    // 0x1F20D
static const uint32_t PGN_BATTERY_STATUS = 127508;   // 0x1F214
static const uint32_t PGN_CONVERTER_STATUS = 127750; // 0x1F306

static const uint16_t VREG_REQUEST = 0x0001;
static const uint16_t VREG_ACK = 0x0002;

// ---------------------------------------------------------------------------------------------
// 29-bit identifier <-> (priority, PGN, source, destination)
// ---------------------------------------------------------------------------------------------
struct CanId {
  uint8_t priority;
  uint32_t pgn;  // PDU1 (PF < 240): PS byte is the destination and is NOT part of the PGN
  uint8_t src;
  uint8_t dst;  // ADDR_GLOBAL for PDU2 (broadcast) PGNs
};

inline CanId decode_id(uint32_t id) {
  CanId r;
  r.src = id & 0xFF;
  r.priority = (id >> 26) & 0x07;
  uint8_t ps = (id >> 8) & 0xFF;
  uint8_t pf = (id >> 16) & 0xFF;
  uint32_t dp = (id >> 24) & 0x03;  // EDP (bit 25) + DP (bit 24)
  if (pf < 240) {
    r.pgn = (dp << 16) | (uint32_t(pf) << 8);
    r.dst = ps;
  } else {
    r.pgn = (dp << 16) | (uint32_t(pf) << 8) | ps;
    r.dst = ADDR_GLOBAL;
  }
  return r;
}

inline uint32_t encode_id(uint8_t priority, uint32_t pgn, uint8_t dst, uint8_t src) {
  uint8_t pf = (pgn >> 8) & 0xFF;
  uint8_t ps = (pf < 240) ? dst : (pgn & 0xFF);
  return (uint32_t(priority & 0x07) << 26) | ((pgn & 0x30000) << 8) | (uint32_t(pf) << 16) | (uint32_t(ps) << 8) | src;
}

// ---------------------------------------------------------------------------------------------
// Little endian readers
// ---------------------------------------------------------------------------------------------
inline uint16_t rd_u16(const uint8_t *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
inline int16_t rd_s16(const uint8_t *p) { return static_cast<int16_t>(rd_u16(p)); }
inline uint32_t rd_u32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline uint64_t rd_u64(const uint8_t *p) { return uint64_t(rd_u32(p)) | (uint64_t(rd_u32(p + 4)) << 32); }

// ---------------------------------------------------------------------------------------------
// ISO 11783-5 address claim
// ---------------------------------------------------------------------------------------------
// NAME layout (64 bit): identity(21) | manufacturer(11) | ecu inst(3) | func inst(5) | function(8) | reserved(1)
//                       | vehicle system(7) | vehicle system inst(4) | industry group(3) | arbitrary address(1)
inline uint64_t make_name(uint32_t identity, uint16_t manufacturer, uint8_t function) {
  uint64_t n = 0;
  n |= uint64_t(identity & 0x1FFFFF);
  n |= uint64_t(manufacturer & 0x7FF) << 21;
  n |= uint64_t(function) << 40;
  n |= uint64_t(4) << 60;  // industry group: marine
  n |= uint64_t(1) << 63;  // arbitrary address capable
  return n;
}
inline uint16_t name_manufacturer(uint64_t name) { return (name >> 21) & 0x7FF; }
inline uint8_t name_function(uint64_t name) { return (name >> 40) & 0xFF; }

// ---------------------------------------------------------------------------------------------
// VREG (Victron register) frames
// ---------------------------------------------------------------------------------------------
// Request one register: 66 99 01 00 regL regH FF FF   (regId + mask 0xFFFF = exact match)
inline void build_vreg_request(uint16_t reg, uint8_t out[8]) {
  out[0] = VREG_HDR0;
  out[1] = VREG_HDR1;
  out[2] = VREG_REQUEST & 0xFF;
  out[3] = VREG_REQUEST >> 8;
  out[4] = reg & 0xFF;
  out[5] = reg >> 8;
  out[6] = 0xFF;
  out[7] = 0xFF;
}

// Write one register: 66 99 regL regH v0 v1 v2 v3   (value little endian, zero padded; un8/un16 use the first bytes)
// e.g. device mode off:        66 99 00 02 04 00 00 00
//      absorption voltage 58 V: 66 99 F7 ED A8 16 00 00
// The device confirms by broadcasting the register with its new value, or refuses with a NACK (reg 0x0002).
inline void build_vreg_write(uint16_t reg, uint32_t value, uint8_t out[8]) {
  out[0] = VREG_HDR0;
  out[1] = VREG_HDR1;
  out[2] = reg & 0xFF;
  out[3] = reg >> 8;
  out[4] = value & 0xFF;
  out[5] = (value >> 8) & 0xFF;
  out[6] = (value >> 16) & 0xFF;
  out[7] = (value >> 24) & 0xFF;
}

struct VregMsg {
  uint16_t reg;
  const uint8_t *data;  // bytes following the 4-byte header
  uint16_t len;
};

// p points at the start of the proprietary payload (66 99 regL regH ...).
inline bool parse_vreg(const uint8_t *p, size_t n, VregMsg &m) {
  if (n < 4 || p[0] != VREG_HDR0 || p[1] != VREG_HDR1)
    return false;
  m.reg = rd_u16(p + 2);
  m.data = p + 4;
  m.len = static_cast<uint16_t>(n - 4);
  return true;
}

// ---------------------------------------------------------------------------------------------
// NMEA 2000 fast-packet reassembly (one in-flight packet per source address)
// ---------------------------------------------------------------------------------------------
// Frame 0: byte0 = seq<<5 | 0, byte1 = total length, bytes 2..7 = first 6 data bytes
// Frame n: byte0 = seq<<5 | n,                       bytes 1..7 = next 7 data bytes
class FastPacketAssembler {
 public:
  static const uint16_t MAX_LEN = 96;  // enough for VREG strings (<= 32 chars) and their header

  // Returns true when a packet has just been completed; then data()/size() are valid
  // until the next call to add_frame().
  bool add_frame(uint8_t src, const uint8_t *d, uint8_t len) {
    if (len < 2)
      return false;
    State &st = this->states_[src];
    uint8_t seq = d[0] >> 5;
    uint8_t idx = d[0] & 0x1F;
    if (idx == 0) {
      uint8_t total = d[1];
      if (total == 0 || total > MAX_LEN) {
        st.active = false;
        return false;
      }
      st.active = true;
      st.seq = seq;
      st.next_idx = 1;
      st.total = total;
      st.got = 0;
      this->append_(st, d + 2, len - 2, 6);
    } else {
      if (!st.active || st.seq != seq || st.next_idx != idx) {
        st.active = false;  // lost frame or interleaved packet: drop it
        return false;
      }
      st.next_idx++;
      this->append_(st, d + 1, len - 1, 7);
    }
    if (st.got >= st.total) {
      st.active = false;
      std::memcpy(this->out_, st.buf, st.total);
      this->out_len_ = st.total;
      return true;
    }
    return false;
  }

  const uint8_t *data() const { return this->out_; }
  uint16_t size() const { return this->out_len_; }

 protected:
  struct State {
    bool active{false};
    uint8_t seq{0};
    uint8_t next_idx{0};
    uint16_t total{0};
    uint16_t got{0};
    uint8_t buf[MAX_LEN];
  };

  void append_(State &st, const uint8_t *src, uint8_t avail, uint8_t max_chunk) {
    uint16_t n = std::min<uint16_t>(std::min<uint16_t>(avail, max_chunk), st.total - st.got);
    std::memcpy(st.buf + st.got, src, n);
    st.got += n;
  }

  std::map<uint8_t, State> states_;
  uint8_t out_[MAX_LEN];
  uint16_t out_len_{0};
};

// ---------------------------------------------------------------------------------------------
// Payload decoders for the standard PGNs used by VE.Can MPPT chargers
// ---------------------------------------------------------------------------------------------
// PGN 127508 Battery Status: [0]=instance, [1..2]=voltage (0.01 V), [3..4]=current (0.1 A, signed),
//                            [5..6]=temperature (0.01 K), [7]=SID.  Not-available: 0xFFFF / 0x7FFF / 0xFFFF.
struct BatteryStatus {
  uint8_t instance;
  bool has_voltage, has_current, has_temperature;
  float voltage;      // V
  float current;      // A
  float temperature;  // degC
};

inline bool decode_battery_status(const uint8_t *d, size_t n, BatteryStatus &o) {
  if (n < 7)
    return false;
  o.instance = d[0];
  uint16_t v = rd_u16(d + 1);
  int16_t i = rd_s16(d + 3);
  uint16_t t = rd_u16(d + 5);
  o.has_voltage = v != 0xFFFF;
  o.has_current = i != 0x7FFF;
  o.has_temperature = t != 0xFFFF;
  o.voltage = o.has_voltage ? v * 0.01f : 0.0f;
  o.current = o.has_current ? i * 0.1f : 0.0f;
  o.temperature = o.has_temperature ? t * 0.01f - 273.15f : 0.0f;
  return true;
}

// PGN 127501 Binary Status Report: [0]=bank instance, then 28 two-bit statuses packed LSB first
// (0 = off, 1 = on, 2 = error, 3 = unavailable).
static const uint8_t BINARY_STATUS_COUNT = 28;
inline bool decode_binary_status(const uint8_t *d, size_t n, uint8_t &instance, uint8_t status[BINARY_STATUS_COUNT]) {
  if (n < 8)
    return false;
  instance = d[0];
  uint64_t bits = 0;
  for (int i = 0; i < 7; i++)
    bits |= uint64_t(d[1 + i]) << (8 * i);
  for (int i = 0; i < BINARY_STATUS_COUNT; i++)
    status[i] = (bits >> (2 * i)) & 0x03;
  return true;
}

// ---------------------------------------------------------------------------------------------
// Text helpers for register values
// ---------------------------------------------------------------------------------------------
// Firmware version, BCD-like: 0x030201 -> "3.02.01", 0x000201 -> "2.01", 0x00C201 -> "C2.01".
inline std::string format_firmware(uint32_t v) {
  // VREG 0x0102 (v23 of "VE.Can registers"): BCD un24. Two significant bytes: "mid.lo" (0x000201 -> 2.01,
  // 0x00C201 -> C2.01, a release candidate). Three bytes: hi.mid, and the low byte is the build type:
  // 0xFF = release (0x0302FF -> 3.02), anything else is a beta build (0x030201 -> 3.02-beta-01).
  if (v == 0xFFFFFF)
    return "none";
  char buf[24];
  uint8_t hi = (v >> 16) & 0xFF, mid = (v >> 8) & 0xFF, lo = v & 0xFF;
  if (hi == 0)
    std::snprintf(buf, sizeof(buf), "%X.%02X", mid, lo);
  else if (lo == 0xFF)
    std::snprintf(buf, sizeof(buf), "%X.%02X", hi, mid);
  else
    std::snprintf(buf, sizeof(buf), "%X.%02X-beta-%02X", hi, mid, lo);
  return buf;
}

// ASCIIZ string out of a register payload.
inline std::string read_asciiz(const uint8_t *d, size_t n) {
  size_t l = 0;
  while (l < n && d[l] != 0)
    l++;
  std::string s(reinterpret_cast<const char *>(d), l);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n'))
    s.pop_back();
  return s;
}

// Device state (VREG 0x0201).
inline const char *device_state_name(uint8_t s) {
  switch (s) {
    case 0x00: return "Off";
    case 0x01: return "Low power";
    case 0x02: return "Fault";
    case 0x03: return "Bulk";
    case 0x04: return "Absorption";
    case 0x05: return "Float";
    case 0x06: return "Storage";
    case 0x07: return "Equalize";
    case 0x08: return "Passthru";
    case 0x09: return "Inverting";
    case 0x0A: return "Assisting";
    case 0x0B: return "Power supply";
    case 0xF5: return "Wake-up";
    case 0xF6: return "Repeated absorption";
    case 0xF7: return "Auto equalize";
    case 0xF8: return "Battery safe";
    case 0xF9: return "Load detect";
    case 0xFA: return "Blocked";
    case 0xFB: return "Test";
    case 0xFC: return "External control";
    case 0xFF: return "Not available";
    default: return "Unknown";
  }
}

// Input MPP mode (VREG 0xEDB3)
inline const char *mppt_mode_name(uint8_t m) {
  switch (m) {
    case 0x00: return "Off";
    case 0x01: return "Limited";  // voltage or current limited
    case 0x02: return "MPP tracking";
    case 0xFF: return "Not available";
    default: return "Unknown";
  }
}

// Charger additional state information (VREG 0xEDD4): comma separated list of the active items.
inline std::string charger_additional_state(uint8_t bits) {
  static const char *const NAMES[8] = {"Safe mode",       "Automatic equalization", "Repeated absorption",
                                       "Low input dimming", "Temperature dimming",  "Sense wire dimming",
                                       "Input current dimming", "Low power mode"};
  std::string out;
  for (int i = 0; i < 8; i++) {
    if (bits & (1 << i)) {
      if (!out.empty())
        out += ", ";
      out += NAMES[i];
    }
  }
  return out.empty() ? "None" : out;
}

// Charger error code (VREG 0xEDDA).
inline const char *charger_error_name(uint8_t e) {
  // v23 of "VE.Can registers", register 0xEDDA
  if (e >= 200 && e <= 254)
    return "Internal error";
  switch (e) {
    case 0: return "No error";
    case 1: return "Battery temperature too high";
    case 2: return "Battery voltage too high";
    case 3: return "Battery temperature sensor miswired (+)";
    case 4: return "Battery temperature sensor miswired (-)";
    case 5: return "Battery temperature sensor disconnected";
    case 6: return "Battery voltage sense miswired (+)";
    case 7: return "Battery voltage sense miswired (-)";
    case 8: return "Battery voltage sense disconnected";
    case 9: return "Battery voltage wire losses too high";
    case 10: return "Battery voltage too low";
    case 11: return "Battery ripple voltage too high";
    case 12: return "Battery low state-of-charge";
    case 13: return "Battery mid-point voltage issue";
    case 14: return "Battery temperature too high";
    case 17: return "Charger temperature too high";
    case 18: return "Charger over-current";
    case 19: return "Charger current reversed";
    case 20: return "Bulk time limit reached";
    case 21: return "Charger current sensor issue";
    case 22: return "Charger temperature sensor miswired";
    case 23: return "Charger temperature sensor disconnected";
    case 24: return "Charger fan missing";
    case 25: return "Charger fan over-current";
    case 26: return "Charger terminal overheated";
    case 27: return "Charger short circuit";
    case 28: return "Charger issue with power stage";
    case 29: return "Charger over-charge protection";
    case 31: return "Input voltage out of range";
    case 32: return "Input voltage too low";
    case 33: return "Input voltage too high";
    case 34: return "Input current too high";
    case 35: return "Input power too high";
    case 36: return "Input polarity reversed";
    case 37: return "Input voltage absent";
    case 38: return "Input shutdown (permanent)";
    case 39: return "Input shutdown (retries)";
    case 40: return "Internal failure (MPPT)";
    case 41: return "Inverter shutdown (panel isolation)";
    case 42: return "Inverter shutdown (ground current)";
    case 43: return "Inverter shutdown (L-PE voltage)";
    case 50: return "Inverter overload";
    case 51: return "Inverter temperature too high";
    case 52: return "Inverter peak current";
    case 53: return "Inverter internal DC level";
    case 54: return "Inverter wrong AC out level";
    case 55: return "Inverter power stage fault";
    case 56: return "Inverter power stage fault";
    case 57: return "Inverter connected to AC";
    case 58: return "Inverter power stage fault";
    case 59: return "AC-in 1 relay test fault";
    case 60: return "AC-in 2 relay test fault";
    case 65: return "Link device missing";
    case 66: return "Link incompatible device (settings)";
    case 67: return "Link BMS connection lost";
    case 68: return "Link network misconfigured";
    case 113: return "Non-volatile storage write error";
    case 114: return "CPU temperature too high";
    case 116: return "Factory calibration data corrupt/lost";
    case 117: return "Incompatible firmware";
    case 118: return "Incompatible hardware";
    case 119: return "User settings corrupt/lost";
    case 120: return "Internal reference voltage failure";
    case 121: return "Tester failure";
    case 122: return "History data invalid";
    default: return "Unknown error";
  }
}

}  // namespace vecan
}  // namespace esphome
