#include "number.h"

namespace esphome {
namespace hm {

void HMPowerPercentNumber::control(float value) {
  this->publish_state(value);
  if (this->parent_ != nullptr) this->parent_->set_power_limit_percent(value);
}

void HMPowerAbsoluteNumber::control(float value) {
  this->publish_state(value);
  if (this->parent_ != nullptr) this->parent_->set_power_limit_absolute(value);
}

}  // namespace hm
}  // namespace esphome
