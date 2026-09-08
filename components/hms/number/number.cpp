#include "number.h"

namespace esphome {
namespace hms {

void HMSPowerPercentNumber::control(float value) {
  this->publish_state(value);
  if (this->parent_ != nullptr) this->parent_->set_power_limit_percent(value);
}

void HMSPowerAbsoluteNumber::control(float value) {
  this->publish_state(value);
  if (this->parent_ != nullptr) this->parent_->set_power_limit_absolute(value);
}

}  // namespace hms
}  // namespace esphome
