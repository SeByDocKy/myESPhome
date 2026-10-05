#include "anker_solix_number.h"

namespace esphome::anker_solix {

static const char *const TAG = "anker_solix.number";

// After a write the battery needs a few seconds to apply the command: the values read back meanwhile are old
static constexpr uint32_t WRITE_HOLD_MS = 15000;

void AnkerPowerNumber::dump_config() { LOG_NUMBER("", "Anker SOLIX Power Setpoint", this); }

void AnkerPowerNumber::control(float value) {
  int32_t watts = static_cast<int32_t>(std::lround(value));
  // The hub clamps the request too (this is only so that the number shows what is really sent)
  const int32_t max_charge = this->parent_->power_limit_charge();
  const int32_t max_discharge = this->parent_->power_limit_discharge();
  if (watts < 0 && max_charge > 0 && -watts > max_charge)
    watts = -max_charge;
  if (watts > 0 && max_discharge > 0 && watts > max_discharge)
    watts = max_discharge;
  this->parent_->write_power_setpoint(watts);
  this->publish_state(static_cast<float>(watts));
}

void AnkerSocNumber::dump_config() {
  LOG_NUMBER("", "Anker SOLIX SOC Limit", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  capability bit: %u  needs backup enable: %s  skip_updates: %u", this->reg_,
                this->capability_bit_, YESNO(this->needs_backup_enable_), this->skip_updates);
}

void AnkerSocNumber::parse_and_publish(const std::vector<uint8_t> &data) {
  if (!this->has_data_(data) || this->held_())
    return;
  this->publish_state(static_cast<float>(this->word_(data, 0)));
}

void AnkerSocNumber::control(float value) {
  const uint16_t reg_value = static_cast<uint16_t>(std::lround(value));
  const char *reason =
      this->parent_->check_soc_write(this->reg_, reg_value, this->capability_bit_, this->needs_backup_enable_);
  if (reason != nullptr) {
    ESP_LOGW(TAG, "'%s' = %u refused: %s", this->get_name().c_str(), static_cast<unsigned>(reg_value), reason);
    if (this->has_state())
      this->publish_state(this->state);  // put the entity back to the value the battery has
    return;
  }
  this->parent_->write_register(this->reg_, reg_value);
  this->parent_->note_soc_written(this->reg_, reg_value);
  this->hold_for(WRITE_HOLD_MS);
  this->publish_state(static_cast<float>(reg_value));
}

}  // namespace esphome::anker_solix
