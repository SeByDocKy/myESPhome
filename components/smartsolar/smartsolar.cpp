#include "smartsolar.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_SENSOR
#include "sensor/smartsolar_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "text_sensor/smartsolar_text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "binary_sensor/smartsolar_binary_sensor.h"
#endif

#ifdef USE_NUMBER
#include "number/smartsolar_number.h"
#endif
#ifdef USE_SWITCH
#include "switch/smartsolar_switch.h"
#endif

#include <cmath>

namespace esphome {
namespace smartsolar {

static const char *const TAG = "smartsolar";

// Victron registers used by this component (see "VE.Can registers - public")
static const uint16_t REG_DEVICE_STATE = 0x0201;
static const uint16_t REG_FIRMWARE = 0x0102;
static const uint16_t REG_SERIAL = 0x010A;
static const uint16_t REG_MODEL = 0x010B;
static const uint16_t REG_CHARGER_ERROR = 0xEDDA;
static const uint16_t REG_INTERNAL_TEMP = 0xEDDB;
static const uint16_t REG_YIELD_TOTAL = 0xEDDD;
static const uint16_t REG_YIELD_TODAY = 0xEDD3;
static const uint16_t REG_MAX_POWER_TODAY = 0xEDD2;
static const uint16_t REG_YIELD_YESTERDAY = 0xEDD1;
static const uint16_t REG_MAX_POWER_YESTERDAY = 0xEDD0;
static const uint16_t REG_ABSORPTION_VOLTAGE = 0xEDF7;
static const uint16_t REG_FLOAT_VOLTAGE = 0xEDF6;
static const uint16_t REG_MAX_CHARGE_CURRENT = 0xEDF0;

// Additional registers (read only), see "VE.Can registers" v23
static const uint16_t REG_INPUT_MPP_MODE = 0xEDB3;
static const uint16_t REG_INPUT_VOLTAGE = 0xEDBB;
static const uint16_t REG_INPUT_POWER = 0xEDBC;
static const uint16_t REG_OUTPUT_VOLTAGE = 0xEDD5;
static const uint16_t REG_OUTPUT_POWER = 0xEDD6;
static const uint16_t REG_OUTPUT_CURRENT = 0xEDD7;
static const uint16_t REG_ADDITIONAL_STATE = 0xEDD4;
static const uint16_t REG_CHARGER_MAX_CURRENT = 0xEDDF;
static const uint16_t REG_BATTERY_TEMPERATURE = 0xEDEC;

static const uint16_t REG_DEVICE_MODE = 0x0200;
static const uint16_t REG_EQUALIZATION_VOLTAGE = 0xEDF4;

static const uint32_t SILENCE_WARNING_MS = 30000;

// Write safety (see SmartSolar::PendingWrite)
static const uint32_t WRITE_DEBOUNCE_MS = 1500;       // numbers: wait for the value to settle
static const uint32_t WRITE_MIN_INTERVAL_MS = 10000;  // minimum spacing between two writes of one register
static const uint32_t WRITE_CONFIRM_MS = 2000;        // wait for the broadcast, then read the register back
static const uint32_t WRITE_VERIFY_MS = 2500;         // wait for the read-back answer
static const uint8_t MODE_OFF = 4;

// Absolute limits of what this component accepts to write, whatever the YAML says (raw register units).
static const uint32_t VOLTAGE_RAW_MIN = 500, VOLTAGE_RAW_MAX = 7000;  // 5.00 .. 70.00 V
static const uint32_t CURRENT_RAW_MIN = 0, CURRENT_RAW_MAX = 1000;    // 0 .. 100.0 A

struct NumberSpec {
  uint16_t reg;
  float scale;
  uint32_t raw_min;
  uint32_t raw_max;
};

[[maybe_unused]] static bool number_spec(uint8_t kind, NumberSpec &s) {
  switch (kind) {
    case NUMBER_ABSORPTION_VOLTAGE: s = {REG_ABSORPTION_VOLTAGE, 0.01f, VOLTAGE_RAW_MIN, VOLTAGE_RAW_MAX}; return true;
    case NUMBER_FLOAT_VOLTAGE: s = {REG_FLOAT_VOLTAGE, 0.01f, VOLTAGE_RAW_MIN, VOLTAGE_RAW_MAX}; return true;
    case NUMBER_EQUALIZATION_VOLTAGE: s = {REG_EQUALIZATION_VOLTAGE, 0.01f, VOLTAGE_RAW_MIN, VOLTAGE_RAW_MAX}; return true;
    case NUMBER_MAX_CHARGE_CURRENT: s = {REG_MAX_CHARGE_CURRENT, 0.1f, CURRENT_RAW_MIN, CURRENT_RAW_MAX}; return true;
    default: return false;
  }
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

// Register backing a sensor kind, 0 when the value comes from a broadcast PGN instead.
static uint16_t reg_for_sensor(uint8_t kind) {
  switch (kind) {
    case SENSOR_ENERGY_TODAY: return REG_YIELD_TODAY;
    case SENSOR_ENERGY_YESTERDAY: return REG_YIELD_YESTERDAY;
    case SENSOR_ENERGY_TOTAL: return REG_YIELD_TOTAL;
    case SENSOR_MAX_POWER_TODAY: return REG_MAX_POWER_TODAY;
    case SENSOR_MAX_POWER_YESTERDAY: return REG_MAX_POWER_YESTERDAY;
    case SENSOR_INTERNAL_TEMPERATURE: return REG_INTERNAL_TEMP;
    case SENSOR_ABSORPTION_VOLTAGE: return REG_ABSORPTION_VOLTAGE;
    case SENSOR_FLOAT_VOLTAGE: return REG_FLOAT_VOLTAGE;
    case SENSOR_MAX_CHARGE_CURRENT: return REG_MAX_CHARGE_CURRENT;
    case SENSOR_INPUT_VOLTAGE: return REG_INPUT_VOLTAGE;
    case SENSOR_INPUT_POWER: return REG_INPUT_POWER;
    case SENSOR_OUTPUT_VOLTAGE: return REG_OUTPUT_VOLTAGE;
    case SENSOR_OUTPUT_CURRENT: return REG_OUTPUT_CURRENT;
    case SENSOR_OUTPUT_POWER: return REG_OUTPUT_POWER;
    case SENSOR_CHARGER_MAX_CURRENT: return REG_CHARGER_MAX_CURRENT;
    case SENSOR_BATTERY_TEMPERATURE_REG: return REG_BATTERY_TEMPERATURE;
    default: return 0;
  }
}

[[maybe_unused]] static uint16_t reg_for_text_sensor(uint8_t kind) {
  switch (kind) {
    case TEXT_STATE: return REG_DEVICE_STATE;
    case TEXT_ERROR: return REG_CHARGER_ERROR;
    case TEXT_FIRMWARE_VERSION: return REG_FIRMWARE;
    case TEXT_MODEL: return REG_MODEL;
    case TEXT_SERIAL_NUMBER: return REG_SERIAL;
    case TEXT_TRACKER_MODE: return REG_INPUT_MPP_MODE;
    case TEXT_ADDITIONAL_STATE: return REG_ADDITIONAL_STATE;
    default: return 0;
  }
}

[[maybe_unused]] static bool is_static_reg(uint16_t reg) { return reg == REG_FIRMWARE || reg == REG_MODEL || reg == REG_SERIAL; }

// ---------------------------------------------------------------------------------------------
// Registration of the platform entities
// ---------------------------------------------------------------------------------------------
#ifdef USE_SENSOR
void SmartSolar::register_sensor(SmartSolarSensor *sensor) {
  this->sensors_.push_back(sensor);
  uint16_t reg = reg_for_sensor(sensor->get_kind());
  if (reg != 0)
    this->poll_regs_.insert(reg);
}
#endif

#ifdef USE_TEXT_SENSOR
void SmartSolar::register_text_sensor(SmartSolarTextSensor *sensor) {
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

#ifdef USE_BINARY_SENSOR
void SmartSolar::register_binary_sensor(SmartSolarBinarySensor *sensor) { this->binary_sensors_.push_back(sensor); }
#endif

#ifdef USE_NUMBER
void SmartSolar::register_number(SmartSolarNumber *number) {
  this->numbers_.push_back(number);
  NumberSpec spec;
  if (number_spec(number->get_kind(), spec))
    this->poll_regs_.insert(spec.reg);
}
#endif

#ifdef USE_SWITCH
void SmartSolar::register_switch(SmartSolarSwitch *sw) {
  this->switches_.push_back(sw);
  if (sw->get_kind() == SWITCH_CHARGER)
    this->poll_regs_.insert(REG_DEVICE_MODE);
}
#endif

// ---------------------------------------------------------------------------------------------
// Publishing helpers
// ---------------------------------------------------------------------------------------------
void SmartSolar::publish_sensor_(uint8_t kind, float value) {
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

void SmartSolar::publish_text_sensor_(uint8_t kind, const std::string &value) {
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

void SmartSolar::publish_binary_sensor_(uint8_t kind, bool value) {
#ifdef USE_BINARY_SENSOR
  for (auto *s : this->binary_sensors_) {
    if (s->get_kind() == kind)
      s->publish_state(value);
  }
#else
  (void) kind;
  (void) value;
#endif
}

// ---------------------------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------------------------
void SmartSolar::setup() {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "No vecan hub configured");
    this->mark_failed();
    return;
  }
  this->boot_ms_ = millis();
  this->parent_->register_device(this);
}

void SmartSolar::dump_config() {
  ESP_LOGCONFIG(TAG, "VE.Can SmartSolar MPPT:");
  ESP_LOGCONFIG(TAG, "  Device address: 0x%02X", this->get_address());
#ifdef USE_NUMBER
  ESP_LOGCONFIG(TAG, "  Writable numbers: %u", static_cast<unsigned>(this->numbers_.size()));
#endif
#ifdef USE_SWITCH
  ESP_LOGCONFIG(TAG, "  Writable switches: %u", static_cast<unsigned>(this->switches_.size()));
#endif
  ESP_LOGCONFIG(TAG, "  Battery instance: %u, PV instance: %u", this->battery_instance_, this->pv_instance_);
  LOG_UPDATE_INTERVAL(this);
}

void SmartSolar::request_(uint16_t reg) {
  if (this->unsupported_.count(reg) != 0)
    return;
  this->parent_->request_vreg(this->get_address(), reg);
}

void SmartSolar::update() {
  if (!this->seen_traffic_ && !this->warned_silent_ && millis() - this->boot_ms_ > SILENCE_WARNING_MS) {
    ESP_LOGW(TAG,
             "No frame received from address 0x%02X yet. Check the address (the vecan hub logs the Victron devices it "
             "finds), the 250 kbps bit rate, the wiring and the bus termination.",
             this->get_address());
    this->warned_silent_ = true;
  }

  if (this->parent_->is_listen_only()) {
    if (!this->warned_listen_only_) {
      ESP_LOGI(TAG, "vecan hub is in listen_only mode: only broadcast values (battery/PV/relay) will be updated");
      this->warned_listen_only_ = true;
    }
    return;
  }
  if (!this->parent_->can_transmit())
    return;  // address claim still in progress

  for (uint16_t reg : this->poll_regs_)
    this->request_(reg);
  for (uint16_t reg : this->static_regs_) {
    if (this->static_done_.count(reg) == 0)
      this->request_(reg);
  }
}

// ---------------------------------------------------------------------------------------------
// Register writes
// ---------------------------------------------------------------------------------------------
SmartSolar::PendingWrite *SmartSolar::find_pending_(uint16_t reg) {
  for (auto &p : this->pending_) {
    if (p.reg == reg)
      return &p;
  }
  return nullptr;
}

void SmartSolar::drop_pending_(uint16_t reg) {
  for (size_t i = 0; i < this->pending_.size(); i++) {
    if (this->pending_[i].reg == reg) {
      this->pending_.erase(this->pending_.begin() + i);
      return;
    }
  }
}

bool SmartSolar::known_raw_(uint16_t reg, uint32_t &raw) const {
  auto it = this->known_values_.find(reg);
  if (it == this->known_values_.end())
    return false;
  raw = it->second;
  return true;
}

// Push the value the charger really has to the number / switch entities backed by `reg`.
void SmartSolar::publish_register_(uint16_t reg, uint32_t raw) {
#ifdef USE_NUMBER
  for (auto *n : this->numbers_) {
    NumberSpec spec;
    if (number_spec(n->get_kind(), spec) && spec.reg == reg)
      n->publish_state(raw * spec.scale);
  }
#endif
#ifdef USE_SWITCH
  if (reg == REG_DEVICE_MODE) {
    for (auto *s : this->switches_) {
      if (s->get_kind() == SWITCH_CHARGER)
        s->publish_state(raw != MODE_OFF);
    }
  }
#endif
  (void) reg;
  (void) raw;
}

bool SmartSolar::queue_write_(uint16_t reg, uint32_t raw, uint32_t debounce_ms) {
  if (this->parent_->is_listen_only()) {
    ESP_LOGW(TAG, "Cannot write register 0x%04X: the vecan hub is in listen_only mode", reg);
    return false;
  }
  if (this->unsupported_.count(reg) != 0) {
    ESP_LOGW(TAG, "Cannot write register 0x%04X: the charger does not support it", reg);
    return false;
  }

  uint32_t now = millis();
  uint32_t known;
  PendingWrite *pw = this->find_pending_(reg);
  if (pw == nullptr && this->known_raw_(reg, known) && known == raw) {
    ESP_LOGD(TAG, "Register 0x%04X already has this value, nothing to write", reg);
    this->publish_register_(reg, raw);
    return true;
  }

  uint32_t due = now + debounce_ms;
  auto last = this->last_write_ms_.find(reg);
  if (last != this->last_write_ms_.end() && now - last->second < WRITE_MIN_INTERVAL_MS) {
    uint32_t earliest = last->second + WRITE_MIN_INTERVAL_MS;
    if (static_cast<int32_t>(earliest - due) > 0)
      due = earliest;
  }

  if (pw != nullptr && pw->state != WriteState::DEBOUNCE) {
    ESP_LOGW(TAG, "A write of register 0x%04X is still being confirmed, ignoring the new request", reg);
    return false;
  }
  if (pw == nullptr) {
    this->pending_.push_back({reg, raw, WriteState::DEBOUNCE, due, false, 0});
  } else {
    pw->raw = raw;
    pw->due_ms = due;
  }
  return true;
}

#ifdef USE_NUMBER
void SmartSolar::write_number(uint8_t kind, float value) {
  NumberSpec spec;
  if (!number_spec(kind, spec) || std::isnan(value))
    return;
  float scaled = std::round(value / spec.scale);
  if (scaled < static_cast<float>(spec.raw_min) || scaled > static_cast<float>(spec.raw_max)) {
    ESP_LOGE(TAG, "Refusing to write %.2f to register 0x%04X: outside %.2f .. %.2f", value, spec.reg,
             spec.raw_min * spec.scale, spec.raw_max * spec.scale);
    uint32_t known;
    if (this->known_raw_(spec.reg, known))
      this->publish_register_(spec.reg, known);
    return;
  }
  uint32_t raw = static_cast<uint32_t>(scaled);

  // float voltage must not be above the absorption voltage
  uint32_t other;
  if (kind == NUMBER_FLOAT_VOLTAGE) {
    PendingWrite *p = this->find_pending_(REG_ABSORPTION_VOLTAGE);
    bool have = p != nullptr ? (other = p->raw, true) : this->known_raw_(REG_ABSORPTION_VOLTAGE, other);
    if (have && raw > other) {
      ESP_LOGE(TAG, "Refusing float voltage %.2f V: it must not exceed the absorption voltage (%.2f V)", value, other * 0.01f);
      uint32_t known;
      if (this->known_raw_(spec.reg, known))
        this->publish_register_(spec.reg, known);
      return;
    }
  } else if (kind == NUMBER_ABSORPTION_VOLTAGE) {
    PendingWrite *p = this->find_pending_(REG_FLOAT_VOLTAGE);
    bool have = p != nullptr ? (other = p->raw, true) : this->known_raw_(REG_FLOAT_VOLTAGE, other);
    if (have && raw < other) {
      ESP_LOGE(TAG, "Refusing absorption voltage %.2f V: it must not be below the float voltage (%.2f V)", value, other * 0.01f);
      uint32_t known;
      if (this->known_raw_(spec.reg, known))
        this->publish_register_(spec.reg, known);
      return;
    }
  }

  if (!this->queue_write_(spec.reg, raw, WRITE_DEBOUNCE_MS)) {
    uint32_t known;
    if (this->known_raw_(spec.reg, known))
      this->publish_register_(spec.reg, known);
  }
}

#endif

#ifdef USE_SWITCH
void SmartSolar::write_switch(uint8_t kind, bool state) {
  if (kind != SWITCH_CHARGER)
    return;
  uint32_t raw = state ? this->on_mode_ : MODE_OFF;
  if (!this->queue_write_(REG_DEVICE_MODE, raw, 0)) {
    uint32_t known;
    if (this->known_raw_(REG_DEVICE_MODE, known))
      this->publish_register_(REG_DEVICE_MODE, known);
  }
}

#endif

void SmartSolar::loop() {
  if (this->pending_.empty())
    return;
  uint32_t now = millis();
  for (size_t i = 0; i < this->pending_.size();) {
    PendingWrite &p = this->pending_[i];
    if (static_cast<int32_t>(now - p.due_ms) < 0) {
      i++;
      continue;
    }
    uint32_t known;
    switch (p.state) {
      case WriteState::DEBOUNCE:
        if (!this->parent_->can_transmit()) {
          i++;  // address claim in progress: try again on the next loop
          continue;
        }
        if (!this->parent_->write_vreg(this->get_address(), p.reg, p.raw)) {
          ESP_LOGW(TAG, "Write of register 0x%04X could not be queued", p.reg);
          if (this->known_raw_(p.reg, known))
            this->publish_register_(p.reg, known);
          this->pending_.erase(this->pending_.begin() + i);
          continue;
        }
        this->last_write_ms_[p.reg] = now;
        p.state = WriteState::SENT;
        p.due_ms = now + WRITE_CONFIRM_MS;
        p.has_seen = false;
        i++;
        break;
      case WriteState::SENT:  // no confirmation broadcast yet: ask for the value
        this->parent_->request_vreg(this->get_address(), p.reg);
        p.state = WriteState::VERIFY;
        p.due_ms = now + WRITE_VERIFY_MS;
        i++;
        break;
      case WriteState::VERIFY: {  // last chance expired
        if (p.has_seen) {
          ESP_LOGE(TAG, "Register 0x%04X was NOT changed: the charger reports 0x%X instead of 0x%X", p.reg, p.seen_raw, p.raw);
          this->publish_register_(p.reg, p.seen_raw);
        } else {
          ESP_LOGE(TAG, "No answer from the charger after writing register 0x%04X, the new value is unconfirmed", p.reg);
          if (this->known_raw_(p.reg, known))
            this->publish_register_(p.reg, known);
        }
        this->pending_.erase(this->pending_.begin() + i);
        break;
      }
    }
  }
}

// A value of a writable register was received (poll answer, confirmation broadcast, ...).
void SmartSolar::on_register_value_(uint16_t reg, uint32_t raw) {
  if (reg == REG_DEVICE_MODE && raw != MODE_OFF && raw != 0 && raw <= 0xFF)
    this->on_mode_ = static_cast<uint8_t>(raw);
  this->known_values_[reg] = raw;

  PendingWrite *p = this->find_pending_(reg);
  if (p == nullptr) {
    this->publish_register_(reg, raw);
    return;
  }
  if (p->state == WriteState::DEBOUNCE)
    return;  // the user is still changing the value: do not move the slider under their finger
  if (raw == p->raw) {
    ESP_LOGI(TAG, "Register 0x%04X confirmed by the charger: 0x%X", reg, raw);
    this->publish_register_(reg, raw);
    this->drop_pending_(reg);
    return;
  }
  // different value: it may be an answer to a poll sent before the write, so only note it, the verdict is given
  // when the verification times out
  p->has_seen = true;
  p->seen_raw = raw;
}

// ---------------------------------------------------------------------------------------------
// Incoming data
// ---------------------------------------------------------------------------------------------
void SmartSolar::on_pgn(uint32_t pgn, const uint8_t *data, uint8_t len) {
  this->seen_traffic_ = true;
  this->last_rx_ms_ = millis();

  if (pgn == vecan::PGN_BATTERY_STATUS) {
    vecan::BatteryStatus st;
    if (!vecan::decode_battery_status(data, len, st))
      return;
    if (st.instance == this->battery_instance_) {
      this->battery_voltage_ = st.has_voltage ? st.voltage : NAN;
      this->battery_current_ = st.has_current ? st.current : NAN;
      this->publish_sensor_(SENSOR_BATTERY_VOLTAGE, this->battery_voltage_);
      this->publish_sensor_(SENSOR_BATTERY_CURRENT, this->battery_current_);
      this->publish_sensor_(SENSOR_BATTERY_POWER, this->battery_voltage_ * this->battery_current_);
      this->publish_sensor_(SENSOR_BATTERY_TEMPERATURE, st.has_temperature ? st.temperature : NAN);
    } else if (st.instance == this->pv_instance_) {
      this->pv_voltage_ = st.has_voltage ? st.voltage : NAN;
      this->pv_current_ = st.has_current ? st.current : NAN;
      this->publish_sensor_(SENSOR_PV_VOLTAGE, this->pv_voltage_);
      this->publish_sensor_(SENSOR_PV_CURRENT, this->pv_current_);
      this->publish_sensor_(SENSOR_PV_POWER, this->pv_voltage_ * this->pv_current_);
    }
  } else if (pgn == vecan::PGN_BINARY_STATUS) {
    uint8_t instance;
    uint8_t status[vecan::BINARY_STATUS_COUNT];
    if (!vecan::decode_binary_status(data, len, instance, status))
      return;
    // BlueSolar MPPT 150/70 & 150/85: 1 relay, 2 alarm, 3 low voltage, 4 high voltage, 5 solar activity
    static const uint8_t KINDS[] = {BINARY_RELAY, BINARY_ALARM, BINARY_LOW_VOLTAGE, BINARY_HIGH_VOLTAGE,
                                    BINARY_SOLAR_ACTIVITY};
    for (uint8_t i = 0; i < sizeof(KINDS); i++) {
      if (status[i] <= 1)  // 2 = error, 3 = unavailable: leave the entity untouched
        this->publish_binary_sensor_(KINDS[i], status[i] == 1);
    }
  }
}

void SmartSolar::on_vreg(uint16_t reg, const uint8_t *data, uint16_t len) {
  this->seen_traffic_ = true;
  this->last_rx_ms_ = millis();

  switch (reg) {
    case REG_DEVICE_STATE:
      if (len >= 1)
        this->publish_text_sensor_(TEXT_STATE, vecan::device_state_name(data[0]));
      break;
    case REG_CHARGER_ERROR:
      if (len >= 1)
        this->publish_text_sensor_(TEXT_ERROR, vecan::charger_error_name(data[0]));
      break;
    case REG_FIRMWARE:
      // un8 identifier (0 = the product itself) followed by un24 version
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

    // un32 registers, 0xFFFFFFFF = not available
    case REG_YIELD_TODAY:
    case REG_YIELD_YESTERDAY:
    case REG_YIELD_TOTAL:
    case REG_MAX_POWER_TODAY:
    case REG_MAX_POWER_YESTERDAY: {
      if (len < 4)
        break;
      uint32_t raw = vecan::rd_u32(data);
      bool na = raw == 0xFFFFFFFF;
      switch (reg) {  // energies are in 0.01 kWh, maximum powers in W
        case REG_YIELD_TODAY: this->publish_sensor_(SENSOR_ENERGY_TODAY, na ? NAN : raw * 0.01f); break;
        case REG_YIELD_YESTERDAY: this->publish_sensor_(SENSOR_ENERGY_YESTERDAY, na ? NAN : raw * 0.01f); break;
        case REG_YIELD_TOTAL: this->publish_sensor_(SENSOR_ENERGY_TOTAL, na ? NAN : raw * 0.01f); break;
        case REG_MAX_POWER_TODAY: this->publish_sensor_(SENSOR_MAX_POWER_TODAY, na ? NAN : static_cast<float>(raw)); break;
        default: this->publish_sensor_(SENSOR_MAX_POWER_YESTERDAY, na ? NAN : static_cast<float>(raw)); break;
      }
      break;
    }

    case REG_INTERNAL_TEMP:  // sn16, 0.01 degC
      if (len >= 2) {
        int16_t raw = vecan::rd_s16(data);
        this->publish_sensor_(SENSOR_INTERNAL_TEMPERATURE, raw == 0x7FFF ? NAN : raw * 0.01f);
      }
      break;
    case REG_ABSORPTION_VOLTAGE:  // un16, 0.01 V (writable)
    case REG_FLOAT_VOLTAGE:
      if (len >= 2) {
        uint16_t raw = vecan::rd_u16(data);
        this->publish_sensor_(reg == REG_ABSORPTION_VOLTAGE ? SENSOR_ABSORPTION_VOLTAGE : SENSOR_FLOAT_VOLTAGE,
                              raw == 0xFFFF ? NAN : raw * 0.01f);
        if (raw != 0xFFFF)
          this->on_register_value_(reg, raw);
      }
      break;
    case REG_MAX_CHARGE_CURRENT:  // un16, 0.1 A (writable)
      if (len >= 2) {
        uint16_t raw = vecan::rd_u16(data);
        this->publish_sensor_(SENSOR_MAX_CHARGE_CURRENT, raw == 0xFFFF ? NAN : raw * 0.1f);
        if (raw != 0xFFFF)
          this->on_register_value_(reg, raw);
      }
      break;
    case REG_EQUALIZATION_VOLTAGE:  // un16, 0.01 V (writable)
      if (len >= 2) {
        uint16_t raw = vecan::rd_u16(data);
        if (raw != 0xFFFF)
          this->on_register_value_(reg, raw);
      }
      break;
    case REG_INPUT_MPP_MODE:
      if (len >= 1)
        this->publish_text_sensor_(TEXT_TRACKER_MODE, vecan::mppt_mode_name(data[0]));
      break;
    case REG_ADDITIONAL_STATE:
      if (len >= 1)
        this->publish_text_sensor_(TEXT_ADDITIONAL_STATE, vecan::charger_additional_state(data[0]));
      break;
    case REG_INPUT_VOLTAGE:   // un16, 0.01 V (0xFFFF = not available)
    case REG_OUTPUT_VOLTAGE:  // un16, 0.01 V
    case REG_OUTPUT_POWER:    // un16, 0.01 W
      if (len >= 2) {
        uint16_t raw = vecan::rd_u16(data);
        uint8_t kind = reg == REG_INPUT_VOLTAGE ? SENSOR_INPUT_VOLTAGE
                       : reg == REG_OUTPUT_VOLTAGE ? SENSOR_OUTPUT_VOLTAGE
                                                   : SENSOR_OUTPUT_POWER;
        this->publish_sensor_(kind, raw == 0xFFFF ? NAN : raw * 0.01f);
      }
      break;
    case REG_OUTPUT_CURRENT:        // un16, 0.1 A
    case REG_CHARGER_MAX_CURRENT:  // un16, 0.1 A
      if (len >= 2) {
        uint16_t raw = vecan::rd_u16(data);
        this->publish_sensor_(reg == REG_OUTPUT_CURRENT ? SENSOR_OUTPUT_CURRENT : SENSOR_CHARGER_MAX_CURRENT,
                              raw == 0xFFFF ? NAN : raw * 0.1f);
      }
      break;
    case REG_INPUT_POWER:  // un32, 0.01 W (0xFFFFFFFF = not available)
      if (len >= 4) {
        uint32_t raw = vecan::rd_u32(data);
        this->publish_sensor_(SENSOR_INPUT_POWER, raw == 0xFFFFFFFF ? NAN : raw * 0.01f);
      }
      break;
    case REG_BATTERY_TEMPERATURE:  // un16, 0.01 K (0xFFFF = not available)
      if (len >= 2) {
        uint16_t raw = vecan::rd_u16(data);
        this->publish_sensor_(SENSOR_BATTERY_TEMPERATURE_REG, raw == 0xFFFF ? NAN : raw * 0.01f - 273.15f);
      }
      break;
    case REG_DEVICE_MODE:  // un8 (writable): 4 = off
      if (len >= 1)
        this->on_register_value_(reg, data[0]);
      break;
    default:
      ESP_LOGV(TAG, "Unhandled register 0x%04X (%u bytes)", reg, len);
      break;
  }
}

void SmartSolar::on_vreg_nack(uint16_t reg, uint16_t code) {
  this->seen_traffic_ = true;
  PendingWrite *pw = this->find_pending_(reg);
  if (pw != nullptr && pw->state != WriteState::DEBOUNCE) {
    ESP_LOGE(TAG, "The charger refused to write register 0x%04X (code 0x%04X: %s)", reg, code, nack_text(code));
    uint32_t known;
    if (this->known_raw_(reg, known))
      this->publish_register_(reg, known);
    this->drop_pending_(reg);
  }
  // 0x80xx / 0x81xx: register or request not supported by this device -> stop asking for it
  if ((code & 0xFF00) == 0x8000 || (code & 0xFF00) == 0x8100) {
    if (this->unsupported_.insert(reg).second) {
      ESP_LOGI(TAG, "Register 0x%04X is not supported by device 0x%02X (code 0x%04X), no longer polling it", reg,
               this->get_address(), code);
    }
  }
}

}  // namespace smartsolar
}  // namespace esphome
