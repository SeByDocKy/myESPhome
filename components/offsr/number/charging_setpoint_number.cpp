#include "charging_setpoint_number.h"

namespace esphome {
namespace offsr {

void ChargingSetpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_activation(state);
}

}  // namespace offsr
}  // namespace esphome
