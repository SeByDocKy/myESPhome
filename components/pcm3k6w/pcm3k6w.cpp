#include "pcm3k6w.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cstdio>

namespace esphome::pcm3k6w {

static const char *const TAG = "pcm3k6w";

// Little-endian 16 bit value starting at data[i] (matches the PCM's
// `(x[i+1] << 8) | x[i]` layout used throughout the original YAML).
static inline uint16_t le16(const std::vector<uint8_t> &d, size_t i) {
  return (static_cast<uint16_t>(d[i + 1]) << 8) | d[i];
}

static const char *running_mode_text(uint8_t mode) {
  switch (mode) {
    case 0: return "Initialize";
    case 1: return "Standby";
    case 2: return "Grid-connected Charging";
    case 3: return "Grid-connected Discharge";
    case 4: return "Off-grid Inverter";
    case 5: return "Fault";
    default: return "Unknown";
  }
}

void PCM3K6WComponent::setup() {
  this->canbus_->add_callback([this](uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data) {
    this->on_frame_(can_id, extended_id, rtr, data);
  });

  // Mirrors the original on_boot sequence: query version + SN once, then
  // after a 2s settle, force the charge/discharge current limits to a safe
  // 1.0A and select charge mode.
  this->set_timeout("pcm3k6w_boot_query", 500, [this]() {
    this->enqueue_query_(0x07);  // program version
    this->enqueue_query_(0x05);  // SN code
  });
  this->set_timeout("pcm3k6w_boot", 2000, [this]() {
    this->enqueue_(0x02, 0x36, {0x0A, 0x00});  // charging current -> 1.0A
    this->enqueue_(0x02, 0x37, {0x0A, 0x00});  // discharging current -> 1.0A
    this->enqueue_(0x02, 0x38, {0x00, 0x00});  // grid mode -> charge
  });

  this->set_interval("pcm3k6w_poll", this->poll_interval_, [this]() { this->poll_(); });
}

void PCM3K6WComponent::loop() {
  if (this->tx_queue_.empty()) return;
  uint32_t now = millis();
  if (now - this->last_tx_time_ < this->frame_gap_) return;

  TxFrame frame = this->tx_queue_.front();
  this->tx_queue_.erase(this->tx_queue_.begin());
  this->canbus_->send_data(frame.can_id, true, false, frame.data);
  this->last_tx_time_ = now;
}

void PCM3K6WComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "PCM3K6W:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  ESP_LOGCONFIG(TAG, "  Poll interval: %ums", this->poll_interval_);
  ESP_LOGCONFIG(TAG, "  Frame gap: %ums", this->frame_gap_);
}

void PCM3K6WComponent::enqueue_(uint8_t cmd_type, uint8_t reg, const std::vector<uint8_t> &payload) {
  TxFrame frame;
  frame.can_id = (static_cast<uint32_t>(cmd_type) << 24) | (static_cast<uint32_t>(this->address_) << 16) |
                 (static_cast<uint32_t>(0x0A) << 8) | reg;
  frame.data.assign(8, 0x00);
  frame.data[0] = this->address_;
  for (size_t i = 0; i < payload.size() && i < 6; i++) frame.data[2 + i] = payload[i];
  this->tx_queue_.push_back(frame);
}

// Periodic configuration/readback poll (mirrors the YAML `interval:` block).
// Live measurements (feedback 1-3, running status, bus voltage) arrive
// unsolicited from the PCM and are handled directly in on_frame_().
void PCM3K6WComponent::poll_() {
  this->enqueue_query_(0x0A);  // phase mode
  this->enqueue_query_(0x11);  // frequency setting
  this->enqueue_query_(0x12);  // AC voltage setting
  this->enqueue_query_(0x13);  // AC voltage protection
  this->enqueue_query_(0x14);  // AC voltage alarm
  this->enqueue_query_(0x31);  // charging voltage
  this->enqueue_query_(0x33);  // DC voltage protection
  this->enqueue_query_(0x34);  // DC voltage alarm
  this->enqueue_query_(0x35);  // fan speed
  this->enqueue_query_(0x36);  // charging current
  this->enqueue_query_(0x37);  // discharging current
}

void PCM3K6WComponent::on_frame_(uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data) {
  if (!extended_id || rtr || data.size() < 7) return;

  uint8_t rx_type = (can_id >> 24) & 0xFF;
  uint8_t rx_module = (can_id >> 16) & 0xFF;
  uint8_t rx_addr = (can_id >> 8) & 0xFF;
  uint8_t rx_reg = can_id & 0xFF;

  // Responses are framed as 0x01 0x0A <address> <reg>.
  if (rx_type != 0x01 || rx_module != 0x0A || rx_addr != this->address_) return;

  switch (rx_reg) {
    case 0x05: {  // SN code response
      char buf[10];
      snprintf(buf, sizeof(buf), "V%d.%02d", data[3], data[2]);
      this->publish_text_sensor_(TXT_SN_CODE, buf);
      break;
    }
    case 0x07: {  // Program version response
      char buf[10];
      snprintf(buf, sizeof(buf), "V%d.%02d", data[3], data[2]);
      this->publish_text_sensor_(TXT_PROGRAM_VERSION, buf);
      break;
    }
    case 0x0A: {  // Phase mode readback
      uint8_t phase = data[2];
      this->publish_sensor_(SENS_PHASE_MODE_READBACK, phase == 0 ? 1.0f : 3.0f);
      if (this->selects_[SEL_PHASE_MODE_SETTING] != nullptr)
        this->selects_[SEL_PHASE_MODE_SETTING]->publish_state(phase == 0 ? "Single-Phase" : "Three-Phase");
      break;
    }
    case 0x11: {  // Frequency setting readback
      uint8_t freq = data[2];
      this->publish_sensor_(SENS_FREQUENCY_SETTING_READBACK, freq == 0 ? 50.0f : 60.0f);
      if (this->selects_[SEL_FREQUENCY_SETTING] != nullptr)
        this->selects_[SEL_FREQUENCY_SETTING]->publish_state(freq == 0 ? "50 Hz" : "60 Hz");
      break;
    }
    case 0x12: {  // AC voltage setting readback
      uint8_t volt = data[2];
      this->publish_sensor_(SENS_AC_VOLTAGE_SETTING_READBACK, volt == 0 ? 230.0f : 240.0f);
      if (this->selects_[SEL_AC_VOLTAGE_SETTING] != nullptr)
        this->selects_[SEL_AC_VOLTAGE_SETTING]->publish_state(volt == 0 ? "230V" : "240V");
      break;
    }
    case 0x13: {  // AC voltage protection readback
      float ov = le16(data, 2) / 10.0f;
      float uv = le16(data, 4) / 10.0f;
      this->publish_sensor_(SENS_AC_OVERVOLTAGE_PROTECTION_READBACK, ov);
      this->publish_sensor_(SENS_AC_UNDERVOLTAGE_PROTECTION_READBACK, uv);
      if (this->numbers_[NUM_AC_OVERVOLTAGE_PROTECTION] != nullptr) this->numbers_[NUM_AC_OVERVOLTAGE_PROTECTION]->publish_state(ov);
      if (this->numbers_[NUM_AC_UNDERVOLTAGE_PROTECTION] != nullptr) this->numbers_[NUM_AC_UNDERVOLTAGE_PROTECTION]->publish_state(uv);
      break;
    }
    case 0x14: {  // AC voltage alarm readback
      float ov = le16(data, 2) / 10.0f;
      float uv = le16(data, 4) / 10.0f;
      this->publish_sensor_(SENS_AC_OVERVOLTAGE_ALARM_READBACK, ov);
      this->publish_sensor_(SENS_AC_UNDERVOLTAGE_ALARM_READBACK, uv);
      break;
    }
    case 0x31: {  // Charging voltage readback
      float volt = le16(data, 2) / 10.0f;
      this->publish_sensor_(SENS_CHARGING_VOLTAGE_READBACK, volt);
      if (this->numbers_[NUM_CHARGING_VOLTAGE] != nullptr) this->numbers_[NUM_CHARGING_VOLTAGE]->publish_state(volt);
      break;
    }
    case 0x33: {  // DC voltage protection readback
      float ov = le16(data, 2) / 10.0f;
      float uv = le16(data, 4) / 10.0f;
      this->publish_sensor_(SENS_DC_OVERVOLTAGE_PROTECTION_READBACK, ov);
      this->publish_sensor_(SENS_DC_UNDERVOLTAGE_PROTECTION_READBACK, uv);
      if (this->numbers_[NUM_DC_OVERVOLTAGE_PROTECTION] != nullptr) this->numbers_[NUM_DC_OVERVOLTAGE_PROTECTION]->publish_state(ov);
      if (this->numbers_[NUM_DC_UNDERVOLTAGE_PROTECTION] != nullptr) this->numbers_[NUM_DC_UNDERVOLTAGE_PROTECTION]->publish_state(uv);
      break;
    }
    case 0x34: {  // DC voltage alarm readback
      float ov = le16(data, 2) / 10.0f;
      float uv = le16(data, 4) / 10.0f;
      this->publish_sensor_(SENS_DC_OVERVOLTAGE_ALARM_READBACK, ov);
      this->publish_sensor_(SENS_DC_UNDERVOLTAGE_ALARM_READBACK, uv);
      break;
    }
    case 0x35: {  // Fan speed readback
      float speed = data[3];
      this->publish_sensor_(SENS_FAN_SPEED_READBACK, speed);
      if (this->numbers_[NUM_FAN_SPEED] != nullptr) this->numbers_[NUM_FAN_SPEED]->publish_state(speed);
      break;
    }
    case 0x36: {  // Charging current readback
      float cur = le16(data, 2) / 10.0f;
      this->publish_sensor_(SENS_CHARGING_CURRENT_READBACK, cur);
      if (this->numbers_[NUM_CHARGING_CURRENT] != nullptr) this->numbers_[NUM_CHARGING_CURRENT]->publish_state(cur);
      break;
    }
    case 0x37: {  // Discharging current readback
      float cur = le16(data, 2) / 10.0f;
      this->publish_sensor_(SENS_DISCHARGING_CURRENT_READBACK, cur);
      if (this->numbers_[NUM_DISCHARGING_CURRENT] != nullptr) this->numbers_[NUM_DISCHARGING_CURRENT]->publish_state(cur);
      break;
    }
    case 0x38: {  // Grid mode readback
      uint8_t mode = data[2];
      this->publish_sensor_(SENS_GRID_MODE_READBACK, mode);
      if (this->selects_[SEL_GRID_MODE] != nullptr)
        this->selects_[SEL_GRID_MODE]->publish_state(mode == 1 ? "Discharge mode" : "Charge mode");
      // Switch semantics (see write_switch/SW_DISCHARGE_CHARGE): ON = charge (mode 0), OFF = discharge (mode 1).
      if (this->switches_[SW_DISCHARGE_CHARGE] != nullptr) this->switches_[SW_DISCHARGE_CHARGE]->publish_state(mode == 0);
      break;
    }
    case 0x51: {  // Feedback parameter 1
      uint8_t running_mode = data[1];
      float grid_v = le16(data, 2) / 10.0f;
      float inv_v = le16(data, 4) / 10.0f;
      float inv_i = le16(data, 6) / 10.0f;
      this->publish_sensor_(SENS_GRID_VOLTAGE, grid_v);
      this->publish_sensor_(SENS_INVERTER_VOLTAGE, inv_v);
      this->publish_sensor_(SENS_INVERTING_CURRENT, inv_i);
      this->publish_sensor_(SENS_RUNNING_MODE_VALUE, running_mode);
      this->publish_text_sensor_(TXT_RUNNING_MODE, running_mode_text(running_mode));
      break;
    }
    case 0x52: {  // Feedback parameter 2
      float dc_v = le16(data, 2) / 10.0f;
      float dc_i = le16(data, 4) / 10.0f;
      float load_i = le16(data, 6) / 10.0f;
      this->publish_sensor_(SENS_DC_VOLTAGE, dc_v);
      this->publish_sensor_(SENS_DC_CURRENT, dc_i);
      this->publish_sensor_(SENS_LOAD_CURRENT, load_i);
      this->publish_sensor_(SENS_DC_POWER, dc_v * dc_i);
      break;
    }
    case 0x53: {  // Feedback parameter 3
      uint8_t temp1 = data[2];
      uint8_t temp2 = data[3];
      float power = le16(data, 4);
      uint8_t warn = data[6];
      this->publish_sensor_(SENS_TEMPERATURE_PFC, temp1);
      this->publish_sensor_(SENS_TEMPERATURE_LLC, temp2);
      this->publish_sensor_(SENS_AC_POWER, power);
      this->publish_binary_sensor_(BSENS_BATTERY_UNDERVOLTAGE_ALARM, warn & 0x01);
      this->publish_binary_sensor_(BSENS_BATTERY_OVERVOLTAGE_ALARM, warn & 0x02);
      this->publish_binary_sensor_(BSENS_GRID_UNDERVOLTAGE_ALARM, warn & 0x04);
      this->publish_binary_sensor_(BSENS_GRID_OVERVOLTAGE_ALARM, warn & 0x08);
      break;
    }
    case 0x54: {  // Running status / fault feedback
      uint8_t inverter_freq = data[2];
      uint8_t f1 = data[4], f2 = data[5], f3 = data[6];
      this->publish_sensor_(SENS_OFFGRID_FREQUENCY, inverter_freq);
      this->publish_text_sensor_(TXT_OFFGRID_FREQUENCY, inverter_freq == 50 ? "50 Hz" : (inverter_freq == 60 ? "60 Hz" : "Unknown"));

      this->publish_binary_sensor_(BSENS_FAULT_SOFT_START_TIMEOUT, f1 & 0x01);
      this->publish_binary_sensor_(BSENS_FAULT_BUS_OVERVOLTAGE, f1 & 0x02);
      this->publish_binary_sensor_(BSENS_FAULT_BUS_UNDERVOLTAGE, f1 & 0x04);
      this->publish_binary_sensor_(BSENS_FAULT_INVERTER_OVERVOLTAGE, f1 & 0x08);
      this->publish_binary_sensor_(BSENS_FAULT_INVERTER_UNDERVOLTAGE, f1 & 0x10);
      this->publish_binary_sensor_(BSENS_FAULT_INVERTER_RMS_OVERCURRENT, f1 & 0x80);

      this->publish_binary_sensor_(BSENS_FAULT_INVERTER_FAST_OVERCURRENT, f2 & 0x01);
      this->publish_binary_sensor_(BSENS_FAULT_DC_OVERVOLTAGE, f2 & 0x02);
      this->publish_binary_sensor_(BSENS_FAULT_DC_UNDERVOLTAGE, f2 & 0x04);
      this->publish_binary_sensor_(BSENS_FAULT_DC_FAST_OVERCURRENT, f2 & 0x08);
      this->publish_binary_sensor_(BSENS_FAULT_DC_RMS_OVERCURRENT, f2 & 0x10);
      this->publish_binary_sensor_(BSENS_FAULT_LOAD_OVERCURRENT, f2 & 0x20);
      this->publish_binary_sensor_(BSENS_FAULT_SOFT_START_SHORT_CIRCUIT, f2 & 0x40);
      this->publish_binary_sensor_(BSENS_FAULT_OUTPUT_OVERLOAD, f2 & 0x80);

      this->publish_binary_sensor_(BSENS_FAULT_OVER_TEMPERATURE, f3 & 0x01);
      this->publish_binary_sensor_(BSENS_FAULT_FAN_FAULT, f3 & 0x02);
      this->publish_binary_sensor_(BSENS_FAULT_SYNC_SIGNAL_LOSS, f3 & 0x04);
      this->publish_binary_sensor_(BSENS_FAULT_LOCKED, f3 & 0x08);
      this->publish_binary_sensor_(BSENS_FAULT_ADDRESS_FAULT, f3 & 0x10);
      this->publish_binary_sensor_(BSENS_FAULT_SN_REPETITION, f3 & 0x20);
      this->publish_binary_sensor_(BSENS_FAULT_LOAD_RMS_OVERCURRENT, f3 & 0x40);
      this->publish_binary_sensor_(BSENS_FAULT_OVERLOAD, f3 & 0x80);

      this->publish_binary_sensor_(BSENS_FAULT_STATUS, (f1 != 0) || (f2 != 0) || (f3 != 0));
      break;
    }
    case 0x72: {  // High voltage bus
      float bus_v = le16(data, 2) / 10.0f;
      this->publish_sensor_(SENS_BUS_VOLTAGE, bus_v);
      break;
    }
    default:
      break;
  }
}

void PCM3K6WComponent::write_switch(uint8_t kind, bool state) {
  switch (kind) {
    case SW_POWER:
      this->enqueue_(0x02, 0x01, {static_cast<uint8_t>(state ? 0x01 : 0x00), 0x00});
      break;
    case SW_DISCHARGE_CHARGE:
      // ON = charge, OFF = discharge (mirrors the original turn_on_action / turn_off_action).
      this->enqueue_(0x02, 0x38, {static_cast<uint8_t>(state ? 0x00 : 0x01), 0x00});
      if (this->selects_[SEL_GRID_MODE] != nullptr)
        this->selects_[SEL_GRID_MODE]->publish_state(state ? "Charge mode" : "Discharge mode");
      break;
    case SW_MANUAL_FAN_CONTROL:
      this->enqueue_(0x02, 0x35, {static_cast<uint8_t>(state ? 0x01 : 0x00), 0x00});
      break;
    default:
      break;
  }
  if (this->switches_[kind] != nullptr) this->switches_[kind]->publish_state(state);
}

void PCM3K6WComponent::write_number(uint8_t kind, float value) {
  // Quantize to the configured number's `step` before doing anything else,
  // so both the CAN payload and the value echoed back to the frontend match
  // - this matters most for NUM_CHARGING_CURRENT / NUM_DISCHARGING_CURRENT,
  // which can also be driven by an `output:` (fan speed 0.0-1.0) whose
  // linear mapping doesn't naturally land on the number's 0.5A step.
  if (kind < NUM_COUNT && this->numbers_[kind] != nullptr) {
    float step = this->numbers_[kind]->traits.get_step();
    if (!std::isnan(step) && step > 0.0f) value = std::round(value / step) * step;
  }

  auto to_u16 = [](float v) -> uint16_t { return static_cast<uint16_t>(v * 10.0f); };
  auto lo = [](uint16_t v) -> uint8_t { return static_cast<uint8_t>(v & 0xFF); };
  auto hi = [](uint16_t v) -> uint8_t { return static_cast<uint8_t>(v >> 8); };

  switch (kind) {
    case NUM_CHARGING_VOLTAGE: {
      uint16_t v = to_u16(value);
      uint16_t cur = to_u16(this->number_state_or_(NUM_CHARGING_CURRENT, 1.0f));
      this->enqueue_(0x02, 0x31, {lo(v), hi(v), lo(cur), hi(cur)});
      this->enqueue_query_(0x31);
      break;
    }
    case NUM_CHARGING_CURRENT: {
      uint16_t v = to_u16(value);
      this->enqueue_(0x02, 0x36, {lo(v), hi(v)});
      break;
    }
    case NUM_DISCHARGING_CURRENT: {
      uint16_t v = to_u16(value);
      this->enqueue_(0x02, 0x37, {lo(v), hi(v)});
      break;
    }
    case NUM_AC_OVERVOLTAGE_PROTECTION: {
      uint16_t ov = to_u16(value);
      uint16_t uv = to_u16(this->number_state_or_(NUM_AC_UNDERVOLTAGE_PROTECTION, 200.0f));
      this->enqueue_(0x02, 0x13, {lo(ov), hi(ov), lo(uv), hi(uv)});
      this->enqueue_query_(0x13);
      break;
    }
    case NUM_AC_UNDERVOLTAGE_PROTECTION: {
      uint16_t uv = to_u16(value);
      uint16_t ov = to_u16(this->number_state_or_(NUM_AC_OVERVOLTAGE_PROTECTION, 250.0f));
      this->enqueue_(0x02, 0x13, {lo(ov), hi(ov), lo(uv), hi(uv)});
      this->enqueue_query_(0x13);
      break;
    }
    case NUM_AC_OVERVOLTAGE_ALARM: {
      uint16_t ov = to_u16(value);
      uint16_t uv = to_u16(this->number_state_or_(NUM_AC_UNDERVOLTAGE_ALARM, 205.0f));
      this->enqueue_(0x02, 0x14, {lo(ov), hi(ov), lo(uv), hi(uv)});
      this->enqueue_query_(0x14);
      break;
    }
    case NUM_AC_UNDERVOLTAGE_ALARM: {
      uint16_t uv = to_u16(value);
      uint16_t ov = to_u16(this->number_state_or_(NUM_AC_OVERVOLTAGE_ALARM, 248.0f));
      this->enqueue_(0x02, 0x14, {lo(ov), hi(ov), lo(uv), hi(uv)});
      this->enqueue_query_(0x14);
      break;
    }
    case NUM_DC_OVERVOLTAGE_PROTECTION: {
      uint16_t ov = to_u16(value);
      uint16_t uv = to_u16(this->number_state_or_(NUM_DC_UNDERVOLTAGE_PROTECTION, 49.0f));
      this->enqueue_(0x02, 0x33, {lo(ov), hi(ov), lo(uv), hi(uv)});
      this->enqueue_query_(0x33);
      break;
    }
    case NUM_DC_UNDERVOLTAGE_PROTECTION: {
      uint16_t uv = to_u16(value);
      uint16_t ov = to_u16(this->number_state_or_(NUM_DC_OVERVOLTAGE_PROTECTION, 58.0f));
      this->enqueue_(0x02, 0x33, {lo(ov), hi(ov), lo(uv), hi(uv)});
      this->enqueue_query_(0x33);
      break;
    }
    case NUM_DC_OVERVOLTAGE_ALARM: {
      uint16_t ov = to_u16(value);
      uint16_t uv = to_u16(this->number_state_or_(NUM_DC_UNDERVOLTAGE_ALARM, 50.0f));
      this->enqueue_(0x02, 0x34, {lo(ov), hi(ov), lo(uv), hi(uv)});
      this->enqueue_query_(0x34);
      break;
    }
    case NUM_DC_UNDERVOLTAGE_ALARM: {
      uint16_t uv = to_u16(value);
      uint16_t ov = to_u16(this->number_state_or_(NUM_DC_OVERVOLTAGE_ALARM, 56.6f));
      this->enqueue_(0x02, 0x34, {lo(ov), hi(ov), lo(uv), hi(uv)});
      this->enqueue_query_(0x34);
      break;
    }
    case NUM_FAN_SPEED: {
      uint8_t activation = value > 0 ? 0x01 : 0x00;
      this->enqueue_(0x02, 0x35, {activation, static_cast<uint8_t>(value)});
      this->enqueue_query_(0x35);
      break;
    }
    default:
      break;
  }
  if (this->numbers_[kind] != nullptr) this->numbers_[kind]->publish_state(value);
}

void PCM3K6WComponent::write_select(uint8_t kind, const std::string &value) {
  switch (kind) {
    case SEL_GRID_MODE: {
      uint8_t mode = (value == "Discharge mode") ? 0x01 : 0x00;
      this->enqueue_(0x02, 0x38, {mode, 0x00});
      this->enqueue_query_(0x38);
      break;
    }
    case SEL_PHASE_MODE_SETTING: {
      uint8_t phase = (value == "Three-Phase") ? 0x01 : 0x00;
      this->enqueue_(0x02, 0x0A, {phase, 0x00});
      ESP_LOGW(TAG, "Phase mode change will take effect on next PCM reboot");
      this->enqueue_query_(0x0A);
      break;
    }
    case SEL_FREQUENCY_SETTING: {
      uint8_t freq = (value == "60 Hz") ? 0x01 : 0x00;
      this->enqueue_(0x02, 0x11, {freq, 0x00});
      ESP_LOGW(TAG, "Frequency change will take effect on next PCM reboot");
      this->enqueue_query_(0x11);
      break;
    }
    case SEL_AC_VOLTAGE_SETTING: {
      uint8_t volt = (value == "240V") ? 0x02 : 0x00;
      this->enqueue_(0x02, 0x12, {volt, 0x00});
      this->enqueue_query_(0x12);
      break;
    }
    default:
      break;
  }
  if (this->selects_[kind] != nullptr) this->selects_[kind]->publish_state(value);
}

void PCM3K6WComponent::write_button(uint8_t kind) {
  switch (kind) {
    case BTN_STOP:
      this->enqueue_(0x02, 0x01, {0x00, 0x00});
      break;
    case BTN_START:
      this->enqueue_(0x02, 0x01, {0x01, 0x00});
      break;
    case BTN_RESET:
      this->enqueue_(0x02, 0x01, {0x02, 0x00});
      break;
    case BTN_SET_DEFAULT_EEPROM: {
      auto to_u16 = [](float v) -> uint16_t { return static_cast<uint16_t>(v * 10.0f); };
      auto lo = [](uint16_t v) -> uint8_t { return static_cast<uint8_t>(v & 0xFF); };
      auto hi = [](uint16_t v) -> uint8_t { return static_cast<uint8_t>(v >> 8); };

      // Same voltage (whatever NUM_CHARGING_VOLTAGE currently reads, or its
      // 56.0V default if that number isn't configured) is reused for both
      // the charging and discharging register - there is no separate
      // discharging voltage entity in this component.
      float voltage = this->number_state_or_(NUM_CHARGING_VOLTAGE, 56.0f);
      uint16_t v = to_u16(voltage);
      uint16_t one_amp = to_u16(1.0f);

      // i) Grid mode (EEPROM, reg 0x02) -> charge.
      this->enqueue_(0x02, 0x02, {0x00, 0x00});
      // ii) Charging voltage/current (EEPROM, reg 0x31) -> current forced to a safe 1.0A.
      this->enqueue_(0x02, 0x31, {lo(v), hi(v), lo(one_amp), hi(one_amp)});
      // iii) Discharging voltage/current (EEPROM, reg 0x32) -> current forced to a safe
      // 1.0A. Unlike 0x31, this register was never exercised by the original YAML (its
      // RX handler was commented out and there was no matching write action) - the
      // voltage field's exact effect on the PCM is unconfirmed, verify on your hardware.
      this->enqueue_(0x02, 0x32, {lo(v), hi(v), lo(one_amp), hi(one_amp)});
      break;
    }
    default:
      break;
  }
}

void PCM3K6WComponent::write_output(uint8_t kind, float value) {
  switch (kind) {
    case OUT_CHARGING_CURRENT:
      this->write_number(NUM_CHARGING_CURRENT, value);
      break;
    case OUT_DISCHARGING_CURRENT:
      this->write_number(NUM_DISCHARGING_CURRENT, value);
      break;
    default:
      break;
  }
}

}  // namespace esphome::pcm3k6w
