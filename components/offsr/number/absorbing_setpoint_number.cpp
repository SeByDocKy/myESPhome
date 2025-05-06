#include "charging_setpoint_number.h"

namespace esphome {
namespace offsr {

void AbsorbingSetpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_absorbing_setpoint(value);
}

}  // namespace offsr
}  // namespace esphome
