#include "charging_setpoint_number.h"

namespace esphome {
namespace offsr {

void ChargingSetpointNumber::setup() {
	float tmp = this->parent_->get_charging_setpoint(); 
	auto call = this->make_call();
	call.set_value(tmp);
	call.perform();
	this->publish_state(tmp);
}

void ChargingSetpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_charging_setpoint(value);
}

}  // namespace offsr
}  // namespace esphome
