#include "number.h"

namespace esphome {
namespace hmsw {

void HMSWPersistentPowerPercentNumber::control(float value) {
  this->publish_state(value);
  if (this->parent_ != nullptr) this->parent_->set_persistent_power_limit_percent(value);
}

}  // namespace hmsw
}  // namespace esphome
