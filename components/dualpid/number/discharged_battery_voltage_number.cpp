#include "esphome/core/version.h"
#include "discharged_battery_voltage_number.h"

namespace esphome::dualpid {

void DischargedBatteryVoltageNumber::setup() {
  float value;
  #if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  // this->pref_ = global_preferences->make_preference<float>(this->get_entity_key());
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  #else
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  #endif
  if (!this->pref_.load(&value)) value = this->parent_->get_discharged_battery_voltage();
  this->parent_->set_discharged_battery_voltage(value);
  this->publish_state(value);
}

void DischargedBatteryVoltageNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_discharged_battery_voltage(value);
  this->pref_.save(&value);
}

}  // namespace esphome
