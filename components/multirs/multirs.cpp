#include "multirs.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_SENSOR
#include "sensor/multirs_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "text_sensor/multirs_text_sensor.h"
#endif
#ifdef USE_SWITCH
#include "switch/multirs_switch.h"
#endif
#ifdef USE_SELECT
#include "select/multirs_select.h"
#endif

#include <cinttypes>
#include <cmath>
#include <cstdio>

namespace esphome {
namespace multirs {

static const char *const TAG = "multirs";

static const uint16_t REG_FIRMWARE = 0x0102;
static const uint16_t REG_SERIAL = 0x010A;
static const uint16_t REG_MODEL = 0x010B;
static const uint16_t REG_DEVICE_MODE = 0x0200;
static const uint16_t REG_DEVICE_STATE = 0x0201;
static const uint16_t REG_ERROR = 0xEDDA;
// Multi RS: AC input 1 control behaviour, 2-bit fields: 0 n/a, 1 yes, 2 no
static const uint16_t REG_AC_IN1_BEHAVIOUR = 0xD067;

static const uint32_t SILENCE_WARNING_MS = 30000;
static const uint32_t WRITE_MIN_INTERVAL_MS = 10000;
static const uint32_t WRITE_CONFIRM_MS = 2000;
static const uint32_t WRITE_VERIFY_MS = 2500;
static const uint8_t SCAN_ROUNDS = 3;

// Device mode (0x0200) as offered by the select. Index = position in the YAML option list, value = register value.
static const uint8_t MODE_VALUES[] = {1, 2, 3, 4, 5};  // charger only, inverter only, on, off, eco

enum class RegType : uint8_t { U16, S16, U32, S32 };

struct SensorSpec {
  uint8_t kind;
  uint16_t reg;
  RegType type;
  float scale;  // default scale, ASSUMED: correct it with a `multiply:` filter if your unit differs
};

static const SensorSpec SENSOR_SPECS[] = {
    {SENSOR_AC_IN_VOLTAGE, 0x2230, RegType::S16, 0.01f},
    {SENSOR_AC_IN_CURRENT, 0x2231, RegType::S16, 0.1f},
    {SENSOR_AC_IN_POWER, 0x2234, RegType::S32, 1.0f},
    {SENSOR_AC_IN_APPARENT_POWER, 0x2235, RegType::S32, 1.0f},
    {SENSOR_AC_IN_FREQUENCY, 0x2238, RegType::U16, 0.01f},
    {SENSOR_AC_OUT_VOLTAGE, 0x2200, RegType::S16, 0.01f},
    {SENSOR_AC_OUT_CURRENT, 0x2201, RegType::S16, 0.1f},
    {SENSOR_AC_OUT_POWER, 0x2204, RegType::S32, 1.0f},
    {SENSOR_AC_OUT_APPARENT_POWER, 0x2205, RegType::S32, 1.0f},
    {SENSOR_AC_OUT_FREQUENCY, 0x2208, RegType::U16, 0.01f},
    {SENSOR_PV_VOLTAGE, 0xEDBB, RegType::U16, 0.01f},
    {SENSOR_PV_POWER, 0xEDBC, RegType::U32, 0.01f},
    {SENSOR_ENERGY_TODAY, 0xEDD3, RegType::U32, 0.01f},
    {SENSOR_ENERGY_YESTERDAY, 0xEDD1, RegType::U32, 0.01f},
    {SENSOR_ENERGY_TOTAL, 0xEDDD, RegType::U32, 0.01f},
    {SENSOR_INTERNAL_TEMPERATURE, 0xEDDB, RegType::S16, 0.01f},
};

[[maybe_unused]] static const SensorSpec *spec_for_kind(uint8_t kind) {
  for (const auto &s : SENSOR_SPECS) {
    if (s.kind == kind)
      return &s;
  }
  return nullptr;
}

// Decode a register value, NaN when the device reports "not available".
static bool decode_value(const SensorSpec &s, const uint8_t *data, uint16_t len, float &out) {
  switch (s.type) {
    case RegType::U16: {
      if (len < 2)
        return false;
      uint16_t v = vecan::rd_u16(data);
      out = v == 0xFFFF ? NAN : v * s.scale;
      return true;
    }
    case RegType::S16: {
      if (len < 2)
        return false;
      int16_t v = vecan::rd_s16(data);
      out = v == 0x7FFF ? NAN : v * s.scale;
      return true;
    }
    case RegType::U32: {
      if (len < 4)
        return false;
      uint32_t v = vecan::rd_u32(data);
      out = v == 0xFFFFFFFF ? NAN : v * s.scale;
      return true;
    }
    case RegType::S32: {
      if (len < 4)
        return false;
      int32_t v = static_cast<int32_t>(vecan::rd_u32(data));
      out = v == 0x7FFFFFFF ? NAN : v * s.scale;
      return true;
    }
  }
  return false;
}

static const char *nack_text(uint16_t code) {
  switch (code) {
    case 0x8000: return "unknown register / invalid request";
    case 0x8100: return "not supported";
    case 0x8200: return "parameter error";
    case 0x8300: return "value out of range";
    case 0x8600: return "not initialised yet";
    case 0xC001: return "controlled by another device";
    default: return "refused";
  }
}

[[maybe_unused]] static uint16_t reg_for_text_sensor(uint8_t kind) {
  switch (kind) {
    case TEXT_STATE: return REG_DEVICE_STATE;
    case TEXT_ERROR: return REG_ERROR;
    case TEXT_FIRMWARE_VERSION: return REG_FIRMWARE;
    case TEXT_MODEL: return REG_MODEL;
    case TEXT_SERIAL_NUMBER: return REG_SERIAL;
    default: return 0;
  }
}

[[maybe_unused]] static bool is_static_reg(uint16_t reg) { return reg == REG_FIRMWARE || reg == REG_MODEL || reg == REG_SERIAL; }

[[maybe_unused]] static uint8_t switch_shift(uint8_t kind) {
  switch (kind) {
    case SWITCH_UPS_FUNCTION: return 0;
    case SWITCH_GENERATOR_LOAD_MODERATION: return 2;
    default: return 4;  // SWITCH_WEAK_AC_INPUT
  }
}

static std::string hex_bytes(const uint8_t *data, uint16_t len) {
  std::string s;
  char b[4];
  for (uint16_t i = 0; i < len && i < 64; i++) {
    snprintf(b, sizeof(b), "%02X", data[i]);
    if (!s.empty())
      s += ' ';
    s += b;
  }
  return s;
}

// ---------------------------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------------------------
#ifdef USE_SENSOR
void MultiRS::register_sensor(MultiRSSensor *sensor) {
  this->sensors_.push_back(sensor);
  const SensorSpec *spec = spec_for_kind(sensor->get_kind());
  if (spec != nullptr)
    this->poll_regs_.insert(spec->reg);
}
#endif

#ifdef USE_TEXT_SENSOR
void MultiRS::register_text_sensor(MultiRSTextSensor *sensor) {
  this->text_sensors_.push_back(sensor);
  uint16_t reg = reg_for_text_sensor(sensor->get_kind());
  if (reg == 0)
    return;
  if (is_static_reg(reg))
    this->static_regs_.insert(reg);
  else
    this->poll_regs_.insert(reg);
}
#endif

#ifdef USE_SWITCH
void MultiRS::register_switch(MultiRSSwitch *sw) {
  this->switches_.push_back(sw);
  this->poll_regs_.insert(REG_AC_IN1_BEHAVIOUR);
}
#endif

#ifdef USE_SELECT
void MultiRS::register_select(MultiRSSelect *select) {
  this->selects_.push_back(select);
  this->poll_regs_.insert(REG_DEVICE_MODE);
}
#endif

void MultiRS::publish_sensor_(uint8_t kind, float value) {
#ifdef USE_SENSOR
  for (auto *s : this->sensors_) {
    if (s->get_kind() == kind)
      s->publish_state(value);
  }
#else
  (void) kind;
  (void) value;
#endif
}

void MultiRS::publish_text_sensor_(uint8_t kind, const std::string &value) {
#ifdef USE_TEXT_SENSOR
  for (auto *s : this->text_sensors_) {
    if (s->get_kind() == kind)
      s->publish_state(value);
  }
#else
  (void) kind;
  (void) value;
#endif
}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------
void MultiRS::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "No vecan hub configured");
    this->mark_failed();
    return;
  }
  this->boot_ms_ = millis();
  this->parent_->register_device(this);
}

void MultiRS::dump_config() {
  ESP_LOGCONFIG(TAG, "VE.Can Multi RS Solar (experimental):");
  ESP_LOGCONFIG(TAG, "  Device address: 0x%02X", this->get_address());
  ESP_LOGCONFIG(TAG, "  Battery instance: %u", this->battery_instance_);
  ESP_LOGCONFIG(TAG, "  Writes enabled: %s", YESNO(this->write_enabled_));
  ESP_LOGCONFIG(TAG, "  Log unknown registers: %s", YESNO(this->log_unknown_));
  for (uint16_t p : this->scan_pages_)
    ESP_LOGCONFIG(TAG, "  Scan page: 0x%04X", p);
  LOG_UPDATE_INTERVAL(this);
}

void MultiRS::request_(uint16_t reg) {
  if (this->unsupported_.count(reg) != 0)
    return;
  this->parent_->request_vreg(this->get_address(), reg);
}

void MultiRS::update() {
  if (!this->seen_traffic_ && !this->warned_silent_ && millis() - this->boot_ms_ > SILENCE_WARNING_MS) {
    ESP_LOGW(TAG,
             "No frame received from address 0x%02X yet. Check the address (the vecan hub logs the Victron devices it "
             "finds), the 250 kbps bit rate, the wiring and the bus termination.",
             this->get_address());
    this->warned_silent_ = true;
  }
  if (this->parent_->is_listen_only()) {
    if (!this->warned_listen_only_) {
      ESP_LOGI(TAG, "vecan hub is in listen_only mode: only the battery values (PGN 127508) will be updated");
      this->warned_listen_only_ = true;
    }
    return;
  }
  if (!this->parent_->can_transmit())
    return;

  for (uint16_t reg : this->poll_regs_)
    this->request_(reg);
  for (uint16_t reg : this->static_regs_) {
    if (this->static_done_.count(reg) == 0)
      this->request_(reg);
  }
  // Discovery: ask for whole register pages (mask 0xFF00); everything received is logged by on_vreg()
  if (this->scan_rounds_ < SCAN_ROUNDS && !this->scan_pages_.empty()) {
    for (uint16_t page : this->scan_pages_)
      this->parent_->request_vreg(this->get_address(), page, 0xFF00);
    this->scan_rounds_++;
  }
}

// ---------------------------------------------------------------------------------------------
// Register writes
// ---------------------------------------------------------------------------------------------
MultiRS::PendingWrite *MultiRS::find_pending_(uint16_t reg) {
  for (auto &p : this->pending_) {
    if (p.reg == reg)
      return &p;
  }
  return nullptr;
}

void MultiRS::drop_pending_(uint16_t reg) {
  for (size_t i = 0; i < this->pending_.size(); i++) {
    if (this->pending_[i].reg == reg) {
      this->pending_.erase(this->pending_.begin() + i);
      return;
    }
  }
}

bool MultiRS::known_raw_(uint16_t reg, uint32_t &raw) const {
  auto it = this->known_values_.find(reg);
  if (it == this->known_values_.end())
    return false;
  raw = it->second;
  return true;
}

void MultiRS::republish_known_(uint16_t reg) {
  uint32_t known;
  if (this->known_raw_(reg, known))
    this->publish_register_(reg, known);
}

void MultiRS::publish_register_(uint16_t reg, uint32_t raw) {
#ifdef USE_SWITCH
  if (reg == REG_AC_IN1_BEHAVIOUR) {
    for (auto *s : this->switches_) {
      uint32_t field = (raw >> switch_shift(s->get_kind())) & 3;
      if (field == 1 || field == 2)  // 0 = not applicable: leave the entity untouched
        s->publish_state(field == 1);
    }
  }
#endif
#ifdef USE_SELECT
  if (reg == REG_DEVICE_MODE) {
    for (auto *s : this->selects_) {
      for (size_t i = 0; i < sizeof(MODE_VALUES); i++) {
        if (MODE_VALUES[i] == raw)
          s->publish_state(i);
      }
    }
  }
#endif
  (void) reg;
  (void) raw;
}

bool MultiRS::queue_write_(uint16_t reg, uint32_t raw) {
  if (!this->write_enabled_) {
    ESP_LOGW(TAG, "Writes are disabled: set `write_enabled: true` in the multirs configuration to change 0x%04X", reg);
    return false;
  }
  if (this->parent_->is_listen_only()) {
    ESP_LOGW(TAG, "Cannot write register 0x%04X: the vecan hub is in listen_only mode", reg);
    return false;
  }
  if (!this->parent_->can_transmit()) {
    ESP_LOGW(TAG, "Cannot write register 0x%04X yet: address claim in progress", reg);
    return false;
  }
  if (this->unsupported_.count(reg) != 0) {
    ESP_LOGW(TAG, "Cannot write register 0x%04X: the device does not support it", reg);
    return false;
  }
  if (this->find_pending_(reg) != nullptr) {
    ESP_LOGW(TAG, "A write of register 0x%04X is still being confirmed, ignoring the new request", reg);
    return false;
  }
  uint32_t known;
  if (this->known_raw_(reg, known) && known == raw) {
    ESP_LOGD(TAG, "Register 0x%04X already has this value, nothing to write", reg);
    this->publish_register_(reg, raw);
    return true;
  }
  uint32_t now = millis();
  auto last = this->last_write_ms_.find(reg);
  if (last != this->last_write_ms_.end() && now - last->second < WRITE_MIN_INTERVAL_MS) {
    ESP_LOGW(TAG, "Register 0x%04X was written less than %u s ago, ignoring the new request", reg,
             static_cast<unsigned>(WRITE_MIN_INTERVAL_MS / 1000));
    return false;
  }
  if (!this->parent_->write_vreg(this->get_address(), reg, raw)) {
    ESP_LOGW(TAG, "Write of register 0x%04X could not be queued", reg);
    return false;
  }
  this->last_write_ms_[reg] = now;
  this->pending_.push_back({reg, raw, WriteState::SENT, now + WRITE_CONFIRM_MS, false, 0});
  return true;
}

#ifdef USE_SWITCH
void MultiRS::write_switch(uint8_t kind, bool state) {
  // The three switches share one register: change only our 2-bit field, keep the others as the device reports them.
  uint32_t known;
  if (!this->known_raw_(REG_AC_IN1_BEHAVIOUR, known)) {
    ESP_LOGW(TAG, "Register 0x%04X has not been read yet, refusing to change it blindly", REG_AC_IN1_BEHAVIOUR);
    return;
  }
  uint8_t shift = switch_shift(kind);
  uint32_t raw = (known & ~(3u << shift)) | ((state ? 1u : 2u) << shift);
  if (!this->queue_write_(REG_AC_IN1_BEHAVIOUR, raw))
    this->republish_known_(REG_AC_IN1_BEHAVIOUR);
}
#endif

#ifdef USE_SELECT
void MultiRS::write_select(uint8_t kind, size_t index) {
  if (kind != SELECT_MODE)
    return;
  if (index >= sizeof(MODE_VALUES)) {
    ESP_LOGE(TAG, "Invalid mode index %u", static_cast<unsigned>(index));
    return;
  }
  if (!this->queue_write_(REG_DEVICE_MODE, MODE_VALUES[index]))
    this->republish_known_(REG_DEVICE_MODE);
}
#endif

void MultiRS::loop() {
  if (this->pending_.empty())
    return;
  uint32_t now = millis();
  for (size_t i = 0; i < this->pending_.size();) {
    PendingWrite &p = this->pending_[i];
    if (static_cast<int32_t>(now - p.due_ms) < 0) {
      i++;
      continue;
    }
    if (p.state == WriteState::SENT) {  // no confirmation broadcast yet: ask for the value
      this->parent_->request_vreg(this->get_address(), p.reg);
      p.state = WriteState::VERIFY;
      p.due_ms = now + WRITE_VERIFY_MS;
      i++;
      continue;
    }
    if (p.has_seen) {
      ESP_LOGE(TAG, "Register 0x%04X was NOT changed: the device reports 0x%" PRIX32 " instead of 0x%" PRIX32, p.reg,
               p.seen_raw, p.raw);
      this->publish_register_(p.reg, p.seen_raw);
    } else {
      ESP_LOGE(TAG, "No answer from the device after writing register 0x%04X, the new value is unconfirmed", p.reg);
      this->republish_known_(p.reg);
    }
    this->pending_.erase(this->pending_.begin() + i);
  }
}

void MultiRS::on_register_value_(uint16_t reg, uint32_t raw) {
  this->known_values_[reg] = raw;
  PendingWrite *p = this->find_pending_(reg);
  if (p == nullptr) {
    this->publish_register_(reg, raw);
    return;
  }
  if (raw == p->raw) {
    ESP_LOGI(TAG, "Register 0x%04X confirmed by the device: 0x%" PRIX32, reg, raw);
    this->publish_register_(reg, raw);
    this->drop_pending_(reg);
    return;
  }
  // may be an answer to a poll sent before the write: the verdict is given when the verification times out
  p->has_seen = true;
  p->seen_raw = raw;
}

// ---------------------------------------------------------------------------------------------
// Incoming data
// ---------------------------------------------------------------------------------------------
void MultiRS::on_pgn(uint32_t pgn, const uint8_t *data, uint8_t len) {
  this->seen_traffic_ = true;
  if (pgn != vecan::PGN_BATTERY_STATUS)
    return;
  vecan::BatteryStatus st;
  if (!vecan::decode_battery_status(data, len, st) || st.instance != this->battery_instance_)
    return;
  float v = st.has_voltage ? st.voltage : NAN;
  float i = st.has_current ? st.current : NAN;
  this->publish_sensor_(SENSOR_BATTERY_VOLTAGE, v);
  this->publish_sensor_(SENSOR_BATTERY_CURRENT, i);
  this->publish_sensor_(SENSOR_BATTERY_POWER, v * i);
  this->publish_sensor_(SENSOR_BATTERY_TEMPERATURE, st.has_temperature ? st.temperature : NAN);
}

void MultiRS::on_vreg(uint16_t reg, const uint8_t *data, uint16_t len) {
  this->seen_traffic_ = true;

  bool in_scan_page = false;
  for (uint16_t p : this->scan_pages_) {
    if ((reg & 0xFF00) == (p & 0xFF00))
      in_scan_page = true;
  }
  if (in_scan_page)
    ESP_LOGI(TAG, "SCAN register 0x%04X (%u bytes): %s", reg, len, hex_bytes(data, len).c_str());

  for (const auto &spec : SENSOR_SPECS) {
    if (spec.reg != reg)
      continue;
    float value;
    if (decode_value(spec, data, len, value))
      this->publish_sensor_(spec.kind, value);
    return;
  }

  switch (reg) {
    case REG_DEVICE_STATE:
      if (len >= 1)
        this->publish_text_sensor_(TEXT_STATE, vecan::device_state_name(data[0]));
      break;
    case REG_ERROR:
      if (len >= 1)
        this->publish_text_sensor_(TEXT_ERROR, vecan::charger_error_name(data[0]));
      break;
    case REG_FIRMWARE:
      if (len >= 4 && data[0] == 0) {
        this->publish_text_sensor_(TEXT_FIRMWARE_VERSION, vecan::format_firmware(vecan::rd_u32(data) >> 8));
        this->static_done_.insert(reg);
      }
      break;
    case REG_MODEL:
      this->publish_text_sensor_(TEXT_MODEL, vecan::read_asciiz(data, len));
      this->static_done_.insert(reg);
      break;
    case REG_SERIAL:
      this->publish_text_sensor_(TEXT_SERIAL_NUMBER, vecan::read_asciiz(data, len));
      this->static_done_.insert(reg);
      break;
    case REG_DEVICE_MODE:  // un8
      if (len >= 1)
        this->on_register_value_(reg, data[0]);
      break;
    case REG_AC_IN1_BEHAVIOUR:  // un16, three 2-bit fields
      if (len >= 2)
        this->on_register_value_(reg, vecan::rd_u16(data));
      break;
    default:
      if (this->log_unknown_ && !in_scan_page) {
        ESP_LOGI(TAG, "Register 0x%04X (%u bytes): %s", reg, len, hex_bytes(data, len).c_str());
      } else {
        ESP_LOGV(TAG, "Unhandled register 0x%04X (%u bytes)", reg, len);
      }
      break;
  }
}

void MultiRS::on_vreg_nack(uint16_t reg, uint16_t code) {
  this->seen_traffic_ = true;
  PendingWrite *pw = this->find_pending_(reg);
  if (pw != nullptr) {
    ESP_LOGE(TAG, "The device refused to write register 0x%04X (code 0x%04X: %s)", reg, code, nack_text(code));
    this->republish_known_(reg);
    this->drop_pending_(reg);
  }
  if ((code & 0xFF00) == 0x8000 || (code & 0xFF00) == 0x8100) {
    if (this->unsupported_.insert(reg).second) {
      ESP_LOGI(TAG, "Register 0x%04X is not supported by device 0x%02X (code 0x%04X), no longer polling it", reg,
               this->get_address(), code);
    }
  }
}

}  // namespace multirs
}  // namespace esphome
