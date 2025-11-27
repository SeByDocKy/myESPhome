#include "charging_setpoint_number.h"

namespace esphome {
namespace bioffsr {

void ChargingSetpointNumber::setup() {	
	float value;
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	if (!this->pref_.load(&value)) value = this->parent_->get_charging_setpoint();
	this->parent_->set_charging_setpoint(value);
	this->publish_state(value);
}

void ChargingSetpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_charging_setpoint(value);
  this->pref_.save(&value);
}

}  // namespace bioffsr
}  // namespace esphome
