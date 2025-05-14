#include "charging_setpoint_number.h"

namespace esphome {
namespace offsr {

void ChargingSetpointNumber::setup() {
 	// float value = this->parent_->get_charging_setpoint(); 
	// auto call = this->make_call();
	// call.set_value(tmp);
	// call.perform();
	
	float value;
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	if (!this->pref_.load(&value)) value= 0.0;
	this->publish_state(value);
	// this->parent_->pid_computed_callback_.call();
    // ESP_LOGV("", "setup: charging_setpoint = %3.2f" , tmp);  
	
}

void ChargingSetpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_charging_setpoint(value);
}

}  // namespace offsr
}  // namespace esphome
