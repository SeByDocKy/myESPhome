#include "charging_setpoint_number.h"

namespace esphome {
namespace offsr {

void ChargingSetpointNumber::setup() {
	auto call = this->make_call();
	call.set_value(this->parent_->get_charging_setpoint());
	call.perform();
	// this->publish_state(this->parent_->get_charging_setpoint());
}

void ChargingSetpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_charging_setpoint(value);
}

}  // namespace offsr
}  // namespace esphome
