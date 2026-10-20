#pragma once

// RIKA Firenet 2.0 USB CDC wire protocol: frame builders, parsers and name tables.
//
// This header has NO ESPHome dependency on purpose, so the protocol layer can be unit tested
// with a plain g++ on a PC. All wire-format knowledge lives here; if a stove family needs a
// different framing, this is the only file to adjust.
//
// Reference: PROTOCOL.md of the open-firenet project (reverse-engineered, interoperability only).

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace esphome {
namespace openfirenet {

// ---------------------------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------------------------

// Stove -> dongle reset probe: 0x16 (SYN) followed by the ASCII character '3' ("version 3" handshake).
static constexpr uint8_t PROBE_SYN = 0x16;
static constexpr char PROBE_VERSION_CHAR = '3';

// The stove only exposes 88 sensor slots (indices 0..87).
static constexpr size_t MAX_SENSOR_SLOTS = 88;

// Core controls read/written positionally: revision, onOff, mode, targetStage, roomTarget.
static constexpr size_t NUM_CONTROLS = 5;

// A 64-byte USB packet is a "full" packet; the stove's USB host aborts the bulk pipe after more than
// 4 full packets in a row. Every frame except the version frame is therefore cut in small pieces.
static constexpr size_t TX_CHUNK_SIZE = 32;
static constexpr size_t SHORT_FRAME_LIMIT = 64;

// Number of TRANSFER_COMPLETED sent per poll cycle: each one dequeues one pending POST_* frame
// (status, controls, sensors), so three drains everything.
static constexpr size_t TRANSFER_COMPLETED_PER_CYCLE = 3;

// Line terminator appended to every dongle -> stove command.
// UNVERIFIED against a real stove: PROTOCOL.md only says "ASCII, delimiter-based". Adjust here if the
// handshake never completes (log the raw frames with `logger: level: VERY_VERBOSE`).
static const char *const EOL = "\n";

// Dongle identity announced in the version frame and status frame. A high BL tells the stove (and the
// cloud) that the dongle is up to date, so the stove never tries to push an OTA firmware onto it.
static constexpr int DONGLE_BL = 999;
static constexpr int DONGLE_APP = 201;
static constexpr int DONGLE_REV = 12201;
static constexpr int DONGLE_SPWF = 229;

// Control field positions (POST_CONTROLS / GET_CONTROLS=1).
enum ControlField : uint8_t {
  CTRL_REVISION = 0,
  CTRL_ON_OFF = 1,
  CTRL_MODE = 2,
  CTRL_TARGET_STAGE = 3,
  CTRL_ROOM_TARGET = 4,
};

// Sensor slot indices confirmed on a real stove (PROTOCOL.md sensor table).
enum SensorSlot : uint8_t {
  SLOT_ROOM_TEMP = 0,
  SLOT_FLAME = 1,
  SLOT_ERR_MASK = 3,
  SLOT_ERR_SUB = 4,
  SLOT_STATE_MASK = 5,
  SLOT_AUGER_SET = 7,
  SLOT_ID_FAN_MEAS = 9,
  SLOT_ID_FAN_SET = 10,
  SLOT_HOPPER_LID_CLOSED = 23,
  SLOT_BOARD_SENSOR = 27,
  SLOT_STAGE_CUR = 30,
  SLOT_MAIN_STATE = 31,
  SLOT_SUB_STATE = 32,
  SLOT_RSSI = 33,
  SLOT_FAB_NUMBER = 35,
  SLOT_MODEL = 36,
  SLOT_APP_VER_BOARD = 38,
  SLOT_FIRMWARE_BUILD = 44,
  SLOT_PELLET_HOURS = 47,
  SLOT_PELLETS_TOTAL = 49,
  SLOT_SERVICE_COUNTDOWN = 50,
  SLOT_IGNITION_COUNT = 53,
  SLOT_ON_OFF_CYCLES = 54,
};

// ---------------------------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------------------------

inline std::string fmt(const char *format, ...) __attribute__((format(printf, 1, 2)));
inline std::string fmt(const char *format, ...) {
  char buf[256];
  va_list args;
  va_start(args, format);
  int n = vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  if (n < 0)
    return std::string();
  if (static_cast<size_t>(n) >= sizeof(buf))
    n = sizeof(buf) - 1;
  return std::string(buf, static_cast<size_t>(n));
}

inline bool starts_with(const std::string &s, const char *prefix) { return s.compare(0, std::strlen(prefix), prefix) == 0; }

inline std::string trim(const std::string &s) {
  size_t b = 0, e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n'))
    b++;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n'))
    e--;
  return s.substr(b, e - b);
}

// strtol without exceptions (ESPHome builds with exceptions disabled).
inline long parse_long(const std::string &s, bool *ok = nullptr) {
  const std::string t = trim(s);
  if (t.empty()) {
    if (ok != nullptr)
      *ok = false;
    return 0;
  }
  char *end = nullptr;
  long v = std::strtol(t.c_str(), &end, 10);
  if (ok != nullptr)
    *ok = (end != nullptr && *end == '\0');
  return v;
}

// Uppercase hex, two digits per byte ("Dieu" -> "44696575"), the SSID encoding used by the stove.
inline std::string to_hex_upper(const std::string &s) {
  static const char *const DIGITS = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size() * 2);
  for (unsigned char c : s) {
    out += DIGITS[c >> 4];
    out += DIGITS[c & 0x0F];
  }
  return out;
}

// Printable rendering of a raw frame for logs (control bytes shown as \xNN, newlines as \n).
inline std::string printable(const std::string &s, size_t max_len = 160) {
  std::string out;
  for (size_t i = 0; i < s.size() && out.size() < max_len; i++) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c == '\n')
      out += "\\n";
    else if (c == '\r')
      out += "\\r";
    else if (c < 0x20 || c > 0x7E)
      out += fmt("\\x%02X", c);
    else
      out += static_cast<char>(c);
  }
  if (s.size() > max_len)
    out += "...";
  return out;
}

// ---------------------------------------------------------------------------------------------
// Frames sent by the dongle
// ---------------------------------------------------------------------------------------------

inline std::string build_version_frame() {
  return fmt("GET_CDCDEVICE3_VERSION=0; BL=%d; APP=%d; REV=%d; DT=3;", DONGLE_BL, DONGLE_APP, DONGLE_REV) + EOL;
}

// The version frame fits in a single short USB packet and is sent whole: cut in two, some stoves
// record a wrong dongle version and offer a stick update.
inline bool is_version_frame(const std::string &f) {
  return f.size() < SHORT_FRAME_LIMIT && starts_with(f, "GET_CDCDEVICE") && f.find("_VERSION=") != std::string::npos;
}

// Registers one nameless-in-practice name per core control (names are ignored by the stove, only
// the position matters).
inline std::string build_controls_read() {
  std::string s = "GET_CONTROLS=0;";
  for (size_t i = 0; i < NUM_CONTROLS; i++)
    s += fmt(" c%u=0;", static_cast<unsigned>(i));
  return s + EOL;
}

// The stove emits exactly as many POST_SENSORS slots as names registered here, starting at slot 0:
// reading slot N requires registering N+1 names.
inline std::string build_sensors_read(size_t slots) {
  std::string s = "GET_SENSORS=0;";
  for (size_t i = 0; i < slots; i++)
    s += fmt(" s%u=0;", static_cast<unsigned>(i));
  return s + EOL;
}

// Mandatory before TRANSFER_COMPLETED: it arms the stove's response slots. The stove ignores the
// revision= and frequency= values, only the presence of the command matters.
inline std::string build_revision_frame(int rssi, long revision) {
  return fmt("GET_REVISION=%d; revision=%ld; frequency=0;", rssi, revision) + EOL;
}

inline std::string build_transfer_completed() { return std::string("TRANSFER_COMPLETED") + EOL; }

// Read-modify-write of the core controls: always send the full set of values.
inline std::string build_controls_write(const std::vector<long> &c) {
  return fmt("GET_CONTROLS=1; revision=%ld; onOff=%ld; mode=%ld; targetStage=%ld; roomTarget=%ld;", c[CTRL_REVISION],
             c[CTRL_ON_OFF], c[CTRL_MODE], c[CTRL_TARGET_STAGE], c[CTRL_ROOM_TARGET]) +
         EOL;
}

struct StatusFields {
  bool wifi_connected{false};  // false -> "blank" status (unprovisioned look on the stove panel)
  int rssi{0};
  std::string id, token;  // echoed from the stove's own POST_CDCDEVICE_STATUS
  std::string ssid_hex;
  std::string wpa2;  // echoed from the stove (never the real WiFi password)
  std::string ip, mac;
};

// GET_CDCDEVICE_STATUS: header line, then the 20 status fields separated by '\n', then the 3 extra
// OTA fields. `symbol` 4 = WiFi connected (icon on the stove panel), 5 = disconnected.
inline std::string build_status_frame(const StatusFields &f) {
  const bool c = f.wifi_connected;
  std::vector<std::string> fields = {
      "0",                                  // 1  monitoring
      "1",                                  // 2  on_off
      "0",                                  // 3  scan_command
      "0",                                  // 4  init_command
      c ? "1" : "0",                        // 5  initialised
      c ? "4" : "5",                        // 6  symbol
      "0",                                  // 7  error
      fmt("%d", DONGLE_BL),                 // 8  bl_version
      fmt("%d", DONGLE_APP),                // 9  app_version
      fmt("%d", DONGLE_REV),                // 10 app_revision
      c ? fmt("%d", DONGLE_SPWF) : "0",     // 11 spwf_version
      c ? fmt("%d", f.rssi) : "0",          // 12 rssi
      c ? f.id : "",                        // 13 id
      c ? f.token : "",                     // 14 token
      "3",                                  // 15 protocol
      c ? f.ssid_hex : "",                  // 16 ssid (hex)
      c ? f.wpa2 : "",                      // 17 wpa2
      c ? f.ip : "",                        // 18 ip
      c ? f.mac : "",                       // 19 mac
      "1",                                  // 20 cdc_device
  };
  std::string s = "GET_CDCDEVICE_STATUS=0;\n";
  for (const auto &v : fields) {
    s += v;
    s += '\n';
  }
  s += "0\n0\n0\n";  // OTA fields
  return s;
}

// ---------------------------------------------------------------------------------------------
// Frames received from the stove
// ---------------------------------------------------------------------------------------------

struct Command {
  std::string name;  // e.g. "POST_SENSORS", "GET_CDCDEVICE_VERSION_FINISHED"
  std::string text;  // the whole segment, starting at the command name
};

inline bool is_name_char(char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

// Splits a received buffer into commands. A frame may carry several commands back to back; a
// command starts at "POST_" or "GET_" (not preceded by a name character) and runs until the next one.
inline void split_commands(const std::string &buf, std::vector<Command> &out) {
  std::vector<size_t> starts;
  for (size_t i = 0; i + 4 <= buf.size(); i++) {
    const bool boundary = (i == 0) || !is_name_char(buf[i - 1]);
    if (!boundary)
      continue;
    if (buf.compare(i, 5, "POST_") == 0 || buf.compare(i, 4, "GET_") == 0)
      starts.push_back(i);
  }
  for (size_t k = 0; k < starts.size(); k++) {
    const size_t b = starts[k];
    const size_t e = (k + 1 < starts.size()) ? starts[k + 1] : buf.size();
    Command cmd;
    cmd.text = buf.substr(b, e - b);
    size_t n = 0;
    while (n < cmd.text.size() && is_name_char(cmd.text[n]))
      n++;
    cmd.name = cmd.text.substr(0, n);
    out.push_back(std::move(cmd));
  }
}

// Positional values of "POST_SENSORS=0; s0=246; s1=580; ..." or "POST_CONTROLS=0; =1; =1; ...":
// the header (up to the first ';') is skipped, then each ';'-separated token contributes the text
// after its '='. Names are ignored, only the position counts.
inline void parse_positional_values(const std::string &segment, std::vector<long> &out) {
  out.clear();
  size_t pos = segment.find(';');
  if (pos == std::string::npos)
    return;
  pos++;
  while (pos < segment.size()) {
    size_t end = segment.find(';', pos);
    if (end == std::string::npos)
      end = segment.size();
    const std::string token = trim(segment.substr(pos, end - pos));
    pos = end + 1;
    if (token.empty())
      continue;
    const size_t eq = token.find('=');
    const std::string value = (eq == std::string::npos) ? token : token.substr(eq + 1);
    out.push_back(parse_long(value));
  }
}

// Lines of a '\n' separated block (CR stripped), header line included as element 0.
inline std::vector<std::string> split_lines(const std::string &s) {
  std::vector<std::string> lines;
  size_t pos = 0;
  while (pos <= s.size()) {
    size_t end = s.find('\n', pos);
    if (end == std::string::npos)
      end = s.size();
    std::string line = s.substr(pos, end - pos);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    lines.push_back(std::move(line));
    pos = end + 1;
  }
  return lines;
}

struct StoveStatus {
  bool valid{false};
  long scan_command{0};
  std::string id, token, ssid_hex, wpa2;
};

// POST_CDCDEVICE_STATUS: header line, 20 '\n' separated fields, terminated by "-------".
inline StoveStatus parse_stove_status(const std::string &segment) {
  StoveStatus st;
  const std::vector<std::string> lines = split_lines(segment);
  std::vector<std::string> fields;
  for (size_t i = 1; i < lines.size(); i++) {
    if (starts_with(lines[i], "-------"))
      break;
    fields.push_back(lines[i]);
  }
  if (fields.size() < 17)
    return st;
  st.valid = true;
  st.scan_command = parse_long(fields[2]);
  st.id = fields[12];
  st.token = fields[13];
  st.ssid_hex = fields[15];
  st.wpa2 = fields[16];
  return st;
}

// ---------------------------------------------------------------------------------------------
// Name tables
// ---------------------------------------------------------------------------------------------

// Main state with its sub-state: state 1 is "Off" or "Standby" according to the sub-state
// (sub-state 0 = switched off, anything else = switched on and waiting for a heat demand).
inline std::string main_state_name(long main_state, long sub_state) {
  switch (main_state) {
    case 0:
      return "Off";
    case 1:
      return sub_state == 0 ? "Off" : "Standby";
    case 2:
      return "Ignition";
    case 3:
      return "Flame Start";
    case 4:
      return "Heating";
    case 5:
      return "Grate Cleaning";
    case 6:
      return "Burn Off";
    case 7:
    case 20:
    case 21:
      return "Split Log";
    case 11:
    case 13:
    case 14:
    case 16:
    case 17:
    case 50:
      return "Split Log Check";
    default:
      return fmt("Unknown (%ld)", main_state);
  }
}

// True while the stove is burning (ignition to burn off, split log modes).
inline bool main_state_burning(long main_state) {
  return (main_state >= 2 && main_state <= 7) || main_state == 20 || main_state == 21;
}

// Active error bitmask (slot 3) rendered as text; "None" when no bit is set.
inline std::string error_text(long mask) {
  if (mask == 0)
    return "None";
  static const char *const NAMES[] = {
      "F00 Ignition failure",       "F01 Flame loss during operation", "F02 Overtemperature",
      "F03 Pellet sensor fault",    "F04 Flue sensor fault",           "F05 Induced-draft fan fault",
  };
  std::string out;
  for (int bit = 0; bit < 32; bit++) {
    if (!(static_cast<uint32_t>(mask) & (1UL << bit)))
      continue;
    if (!out.empty())
      out += ", ";
    out += (bit < 6) ? std::string(NAMES[bit]) : fmt("Error bit %d", bit);
  }
  return out;
}

inline const char *rika_model_name(long id) {
  switch (id) {
    case 1: return "INDUO";
    case 2: return "TOPO";
    case 3: return "ROCO";
    case 4: return "ROCO MULTIAIR";
    case 5: return "ROCO RAO";
    case 6: return "KAPO";
    case 7: return "MIRO";
    case 8: return "COMO";
    case 9: return "REVO";
    case 10: return "INTERNO";
    case 11: return "FILO";
    case 12: return "SUMO";
    case 13: return "DOMO";
    case 14: return "CORSO";
    case 15: return "INDUO II";
    case 16: return "REVIVO";
    case 17: return "PARO";
    case 18: return "LIVO";
    case 19: return "COMO II";
    case 20: return "REVO II";
    case 21: return "COSMO";
    case 22: return "SONO";
    case 23: return "DOMO BACK";
    case 24: return "PK E";
    case 25: return "SUMO MULTIAIR";
    case 26: return "CONNECT";
    default: return nullptr;
  }
}

inline const char *animo_model_name(long id) {
  switch (id) {
    case 1: return "AVITO";
    case 2: return "AVITO SLIM";
    case 3: return "AVITO RAO";
    case 4: return "ADEVO";
    case 5: return "PURE";
    case 6: return "ADUO";
    case 7: return "AMITO";
    case 8: return "ARONDO";
    case 9: return "ADUO 2";
    default: return nullptr;
  }
}

// `make` is the "fabNumber" slot: 1 = RIKA, 2 = ANIMO.
inline std::string model_name(long model_id, long make) {
  if (make == 2) {
    const char *n = animo_model_name(model_id);
    return n != nullptr ? std::string("ANIMO ") + n : fmt("ANIMO (model %ld)", model_id);
  }
  const char *n = rika_model_name(model_id);
  return n != nullptr ? std::string("RIKA ") + n : fmt("RIKA (model %ld)", model_id);
}

// Mainboard version 229 + firmware build 58512 -> "V2.29.585.12".
inline std::string firmware_text(long board, long build) {
  return fmt("V%ld.%02ld.%ld.%02ld", board / 100, board % 100, build / 100, build % 100);
}

}  // namespace openfirenet
}  // namespace esphome
