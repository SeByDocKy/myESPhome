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

static const uint32_t SILENCE_WARNING_MS = 30000;

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
    default: return 0;
  }
}

static uint16_t reg_for_text_sensor(uint8_t kind) {
  switch (kind) {
    case TEXT_STATE: return REG_DEVICE_STATE;
    case TEXT_ERROR: return REG_CHARGER_ERROR;
    case TEXT_FIRMWARE_VERSION: return REG_FIRMWARE;
    case TEXT_MODEL: return REG_MODEL;
    case TEXT_SERIAL_NUMBER: return REG_SERIAL;
    default: return 0;
  }
}

static bool is_static_reg(uint16_t reg) { return reg == REG_FIRMWARE || reg == REG_MODEL || reg == REG_SERIAL; }

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
    case REG_ABSORPTION_VOLTAGE:  // un16, 0.01 V
    case REG_FLOAT_VOLTAGE:
      if (len >= 2) {
        uint16_t raw = vecan::rd_u16(data);
        this->publish_sensor_(reg == REG_ABSORPTION_VOLTAGE ? SENSOR_ABSORPTION_VOLTAGE : SENSOR_FLOAT_VOLTAGE,
                              raw == 0xFFFF ? NAN : raw * 0.01f);
      }
      break;
    case REG_MAX_CHARGE_CURRENT:  // un16, 0.1 A
      if (len >= 2) {
        uint16_t raw = vecan::rd_u16(data);
        this->publish_sensor_(SENSOR_MAX_CHARGE_CURRENT, raw == 0xFFFF ? NAN : raw * 0.1f);
      }
      break;
    default:
      ESP_LOGV(TAG, "Unhandled register 0x%04X (%u bytes)", reg, len);
      break;
  }
}

void SmartSolar::on_vreg_nack(uint16_t reg, uint16_t code) {
  this->seen_traffic_ = true;
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
