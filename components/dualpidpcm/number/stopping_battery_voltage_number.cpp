#include "stopping_battery_voltage_number.h"

namespace esphome {
namespace dualpidpcm {

void StoppingBatteryVoltageNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_stopping_battery_voltage();
  this->parent_->set_stopping_battery_voltage(value);
  this->publish_state(value);
}

void StoppingBatteryVoltageNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_stopping_battery_voltage(value);
  this->pref_.save(&value);
}

}  // namespace dualpidpcm
}  // namespace esphome
