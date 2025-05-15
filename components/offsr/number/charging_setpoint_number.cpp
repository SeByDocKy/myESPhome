#include "charging_setpoint_number.h"

namespace esphome {
namespace offsr {

// static const char *const TAG = "OFFSR.number";

void ChargingSetpointNumber::setup() {
 	// float value = this->parent_->get_charging_setpoint(); 
	// auto call = this->make_call();
	// call.set_value(tmp);
	// call.perform();
	
	float value;
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	if (!this->pref_.load(&value)) value = this->parent_->get_charging_setpoint();
	this->publish_state(value);
	
}

/* void ChargingSetpointNumber::dump_config() {
  ESP_LOGCONFIG(TAG, "Charging Setpoint Number", this->get_name().c_str());
} */


void ChargingSetpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_charging_setpoint(value);
}

}  // namespace offsr
}  // namespace esphome
