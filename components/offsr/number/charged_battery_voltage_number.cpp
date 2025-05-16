#include "charged_battery_voltage_number.h"

namespace esphome {
namespace offsr {

void ChargedBatteryVoltageNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_charged_battery_voltage();
  this->parent_->set_charged_battery_voltage(value);
  this->publish_state(value);	
}

void ChargedBatteryVoltageNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_charged_battery_voltage(value);
  this->pref_.save(&value);
}

}  // namespace offsr
}  // namespace esphome
