#include "floating_setpoint_number.h"

namespace esphome {
namespace offsr {

void FloatingSetpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_floating_setpoint(value);
}

}  // namespace offsr
}  // namespace esphome
