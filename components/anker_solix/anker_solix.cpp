#include "anker_solix.h"

#include <array>

namespace esphome::anker_solix {

static const char *const TAG = "anker_solix";

// After a write the battery needs a few seconds to apply the command: the values read back meanwhile are old
static constexpr uint32_t WRITE_HOLD_MS = 15000;
// The third party mode is not selected again more often than this when the battery does not report it
static constexpr uint32_t MODE_WRITE_MIN_INTERVAL_MS = 10000;
// A warning about an unknown power limit is not repeated more often than this
static constexpr uint32_t LIMIT_WARNING_INTERVAL_MS = 30000;

void AnkerSolixHub::setup() {
  if (this->controller_ == nullptr) {
    ESP_LOGE(TAG, "No modbus_controller: nothing to talk to the battery with");
    this->mark_failed();
    return;
  }
  if (this->slow_controller_ == nullptr)
    this->slow_controller_ = this->controller_;

  this->mode_writer_.init(this->controller_);
  this->setpoint_writer_.init(this->controller_);
  this->soc_writer_.init(this->controller_);
  this->other_writer_.init(this->controller_);

  // The hub adds its own readers here, before the controllers build their read ranges from the registered items
  // (this hub's setup priority is higher than theirs). Everything else registered at code generation time.
  if (this->auto_third_party_control_)
    this->track_mode_ = true;  // it needs to know the current mode

  // Battery status: the first reply tells that the battery answers (the controller only reports the changes)
  this->heartbeat_reader_.configure(10001, 1, SensorValueType::U_WORD, EntityType::INPUT_REGISTER);
  this->heartbeat_reader_.set_callback([this](int64_t) { this->set_connected_(true); });
  this->add_item(&this->heartbeat_reader_, false);

  if (this->track_mode_) {
    this->mode_reader_.configure(REG_OPERATING_MODE, 1, SensorValueType::U_WORD, EntityType::HOLDING);
    this->mode_reader_.set_callback([this](int64_t value) { this->mode_ = static_cast<int32_t>(value); });
    this->add_item(&this->mode_reader_, false);

    this->ems_mask_reader_.configure(REG_EMS_MODE_MASK, 1, SensorValueType::U_WORD, EntityType::INPUT_REGISTER);
    this->ems_mask_reader_.set_callback([this](int64_t value) { this->ems_mask_ = static_cast<int32_t>(value); });
    this->add_item(&this->ems_mask_reader_, true);
  }

  if (this->power_limits_) {
    this->max_charge_reader_.configure(REG_MAX_CHARGE_POWER, 2, SensorValueType::S_DWORD, EntityType::INPUT_REGISTER);
    // The sign of the maximum charge power is not documented: only its size matters
    this->max_charge_reader_.set_callback(
        [this](int64_t value) { this->max_charge_w_ = static_cast<int32_t>(value < 0 ? -value : value); });
    this->add_item(&this->max_charge_reader_, true);

    this->max_discharge_reader_.configure(REG_MAX_DISCHARGE_POWER, 2, SensorValueType::S_DWORD,
                                          EntityType::INPUT_REGISTER);
    this->max_discharge_reader_.set_callback(
        [this](int64_t value) { this->max_discharge_w_ = static_cast<int32_t>(value < 0 ? -value : value); });
    this->add_item(&this->max_discharge_reader_, true);
  }

  if (this->soc_rules_) {
    for (uint8_t i = 0; i < 3; i++) {
      this->soc_readers_[i].configure(REG_CHARGING_LIMIT_SOC + i, 1, SensorValueType::U_WORD, EntityType::HOLDING);
      this->soc_readers_[i].set_callback([this, i](int64_t value) { this->soc_[i] = static_cast<int32_t>(value); });
      this->add_item(&this->soc_readers_[i], true);
    }
    this->backup_reader_.configure(REG_BACKUP_SOC_ENABLE, 1, SensorValueType::U_WORD, EntityType::HOLDING);
    this->backup_reader_.set_callback([this](int64_t value) { this->backup_enable_ = static_cast<int32_t>(value); });
    this->add_item(&this->backup_reader_, true);

    // Register 0x8007 is read on its own, as the official integration does: a read that spans it gets a silent
    // zero appended by the firmware that does not implement it, which would look like "no function supported".
    // A firmware without it answers "Illegal data address" to this read: set read_capability_mask: false then (the
    // SOC limit functions are assumed to be supported, as the official integration does).
    if (this->read_capability_mask_) {
      this->capability_reader_.configure(REG_PARALLEL_CAPABILITY_MASK, 1, SensorValueType::U_WORD,
                                         EntityType::INPUT_REGISTER);
      this->capability_reader_.set_own_range(true);
      this->capability_reader_.set_callback(
          [this](int64_t value) { this->capability_mask_ = static_cast<int32_t>(value); });
      this->add_item(&this->capability_reader_, true);
    }
  }

  this->controller_->add_on_online_callback([this](int, int) { this->set_connected_(true); });
  this->controller_->add_on_offline_callback([this](int, int) { this->set_connected_(false); });
  this->set_connected_(false);
}

void AnkerSolixHub::add_item(SensorItem *item, bool slow) {
  ModbusController *controller = slow && this->slow_controller_ != nullptr ? this->slow_controller_ : this->controller_;
  if (controller != nullptr)
    controller->add_sensor_item(item);
}

void AnkerSolixHub::loop() {
  if (this->setpoint_refresh_ms_ != 0 && this->setpoint_valid_ &&
      millis() - this->last_setpoint_ms_ >= this->setpoint_refresh_ms_) {
    ESP_LOGD(TAG, "Re-sending the power setpoint (%d W)", static_cast<int>(this->setpoint_w_));
    this->ensure_third_party_control_();
    this->send_power_setpoint_(this->setpoint_w_);
  }
}

void AnkerSolixHub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Anker SOLIX:\n"
                "  Model: %s\n"
                "  Auto Third-Party Controlled mode: %s\n"
                "  Max charge power (YAML): %u W\n"
                "  Max discharge power (YAML): %u W\n"
                "  Setpoint refresh: %u ms\n"
                "  Read capability mask: %s\n"
                "  Slow controller: %s",
                this->model_, YESNO(this->auto_third_party_control_), static_cast<unsigned>(this->max_charge_override_w_),
                static_cast<unsigned>(this->max_discharge_override_w_),
                static_cast<unsigned>(this->setpoint_refresh_ms_), YESNO(this->read_capability_mask_),
                YESNO(this->slow_controller_ != nullptr && this->slow_controller_ != this->controller_));
}

void AnkerSolixHub::set_connected_(bool connected) {
  if (this->connected_known_ && this->connected_ == connected)
    return;
  this->connected_known_ = true;
  this->connected_ = connected;
  if (connected) {
    ESP_LOGI(TAG, "Battery answers (%s)", this->model_);
  }
#ifdef USE_BINARY_SENSOR
  if (this->connection_sensor_ != nullptr)
    this->connection_sensor_->publish_state(connected);
#endif
}

void AnkerSolixHub::write_register(uint16_t reg, uint16_t value) {
  ESP_LOGD(TAG, "Write register %u = %u (0x%04X)", reg, value, value);
  if (!this->writer_for_(reg).write_single_register(reg, value))
    ESP_LOGW(TAG, "Write of register %u refused by the Modbus bus (queue full?)", reg);
}

AnkerSolixWriter &AnkerSolixHub::writer_for_(uint16_t reg) {
  if (reg == REG_OPERATING_MODE)
    return this->mode_writer_;
  if (reg >= REG_CHARGING_LIMIT_SOC && reg <= REG_BACKUP_SOC_ENABLE)
    return this->soc_writer_;
  return this->other_writer_;
}

void AnkerSolixHub::send_power_setpoint_(int32_t watts) {
  // INT32, two's complement, high register first
  const uint32_t raw = static_cast<uint32_t>(watts);
  const std::array<uint16_t, 2> words{static_cast<uint16_t>(raw >> 16), static_cast<uint16_t>(raw & 0xFFFF)};
  ESP_LOGD(TAG, "Write power setpoint %d W (registers %u..%u = 0x%04X 0x%04X)", static_cast<int>(watts),
           REG_POWER_SETPOINT, REG_POWER_SETPOINT + 1, words[0], words[1]);
  if (!this->setpoint_writer_.write_multiple_registers(REG_POWER_SETPOINT, std::span<const uint16_t>(words)))
    ESP_LOGW(TAG, "Write of the power setpoint refused by the Modbus bus (queue full?)");
  this->setpoint_w_ = watts;
  this->setpoint_valid_ = true;
  this->last_setpoint_ms_ = millis();
}

void AnkerSolixHub::write_power_setpoint(int32_t watts) {
  // Never ask for more than the battery can do (what it reports, and the limits of the YAML)
  const int64_t requested = watts;
  const int32_t max_charge = this->power_limit_charge();
  const int32_t max_discharge = this->power_limit_discharge();
  if (requested < 0 && max_charge > 0 && -requested > max_charge) {
    ESP_LOGD(TAG, "Charge setpoint %d W limited to %d W", static_cast<int>(watts), static_cast<int>(max_charge));
    watts = -max_charge;
  } else if (requested > 0 && max_discharge > 0 && requested > max_discharge) {
    ESP_LOGD(TAG, "Discharge setpoint %d W limited to %d W", static_cast<int>(watts), static_cast<int>(max_discharge));
    watts = max_discharge;
  }
  this->ensure_third_party_control_();
  this->send_power_setpoint_(watts);
}

void AnkerSolixHub::set_power_fraction(bool charge, float fraction) {
  if (!(fraction >= 0.0f))  // also catches NaN
    fraction = 0.0f;
  if (fraction > 1.0f)
    fraction = 1.0f;
  (charge ? this->charge_fraction_ : this->discharge_fraction_) = fraction;

  const int32_t max_charge = this->power_limit_charge();
  const int32_t max_discharge = this->power_limit_discharge();
  if ((this->charge_fraction_ > 0.0f && max_charge <= 0) || (this->discharge_fraction_ > 0.0f && max_discharge <= 0)) {
    const uint32_t now = millis();
    if (now - this->last_limit_warning_ms_ >= LIMIT_WARNING_INTERVAL_MS || this->last_limit_warning_ms_ == 0) {
      this->last_limit_warning_ms_ = now == 0 ? 1 : now;
      ESP_LOGW(TAG, "The maximum %s power is not known yet (registers 10036 / 10038 not read, no max_*_power in "
                    "the YAML): the output is ignored",
               charge ? "charge" : "discharge");
    }
    return;
  }
  const int32_t request = static_cast<int32_t>(std::lround(this->discharge_fraction_ * static_cast<float>(max_discharge))) -
                          static_cast<int32_t>(std::lround(this->charge_fraction_ * static_cast<float>(max_charge)));
  this->write_power_setpoint(request);
}

// The setpoint is only applied in the "Third-Party Controlled" mode
void AnkerSolixHub::ensure_third_party_control_() {
  if (!this->auto_third_party_control_ || this->mode_ == MODE_THIRD_PARTY_CONTROL)
    return;
  const uint32_t now = millis();
  if (this->mode_write_sent_ && now - this->last_mode_write_ms_ < MODE_WRITE_MIN_INTERVAL_MS)
    return;
  ESP_LOGI(TAG, "Selecting the Third-Party Controlled mode (operating mode was %d)", static_cast<int>(this->mode_));
  this->write_register(REG_OPERATING_MODE, MODE_THIRD_PARTY_CONTROL);
  this->mode_ = MODE_THIRD_PARTY_CONTROL;  // read back by the next poll, which is ignored while the battery switches
  this->mode_reader_.hold_for(WRITE_HOLD_MS);
  this->mode_write_sent_ = true;
  this->last_mode_write_ms_ = now;
}

bool AnkerSolixHub::request_operating_mode(uint16_t mode, uint8_t capability_bit) {
  if (this->ems_mask_ >= 0 && ((this->ems_mask_ >> capability_bit) & 1) == 0) {
    ESP_LOGW(TAG, "Operating mode %u refused: bit %u of the EMS mode mask (0x%04X) says this device does not support it",
             mode, capability_bit, static_cast<unsigned>(this->ems_mask_));
    return false;
  }
  this->write_register(REG_OPERATING_MODE, mode);
  this->mode_ = mode;
  this->mode_reader_.hold_for(WRITE_HOLD_MS);
  this->mode_write_sent_ = true;
  this->last_mode_write_ms_ = millis();
  return true;
}

// The rules of the official integration (config/*.yaml: capability_bit, soc_validation, write_condition)
const char *AnkerSolixHub::check_soc_write(uint16_t reg, uint16_t value, uint8_t capability_bit,
                                           bool needs_backup_enable) {
  if (this->capability_mask_ >= 0 && ((this->capability_mask_ >> capability_bit) & 1) == 0)
    return "this device does not support it (bit of the capability mask, register 0x8007)";
  if (needs_backup_enable && this->backup_enable_ == 0)
    return "the backup SOC function is disabled: enable it first in the Anker app";
  // The ordering rules only apply while the backup SOC function is enabled
  if (this->backup_enable_ != 1)
    return nullptr;

  const int32_t charging = this->soc_[0];
  const int32_t discharge = this->soc_[1];
  const int32_t reserve = this->soc_[2];
  switch (reg) {
    case REG_CHARGING_LIMIT_SOC:
      if (discharge >= 0 && value <= discharge)
        return "the charging limit must be greater than the discharge limit";
      if (reserve >= 0 && value <= reserve)
        return "the charging limit must be greater than the backup reserve";
      break;
    case REG_DISCHARGE_LIMIT_SOC:
      if (reserve >= 0 && value > reserve)
        return "the discharge limit must not exceed the backup reserve";
      if (charging >= 0 && value >= charging)
        return "the discharge limit must be lower than the charging limit";
      break;
    case REG_BACKUP_RESERVE_SOC:
      if (discharge >= 0 && value < discharge)
        return "the backup reserve must not be lower than the discharge limit";
      if (charging >= 0 && value >= charging)
        return "the backup reserve must be lower than the charging limit";
      break;
    default:
      break;
  }
  return nullptr;
}

void AnkerSolixHub::note_soc_written(uint16_t reg, uint16_t value) {
  if (reg < REG_CHARGING_LIMIT_SOC || reg > REG_BACKUP_RESERVE_SOC)
    return;
  const uint8_t index = static_cast<uint8_t>(reg - REG_CHARGING_LIMIT_SOC);
  this->soc_[index] = value;
  this->soc_readers_[index].hold_for(WRITE_HOLD_MS);
}

}  // namespace esphome::anker_solix
