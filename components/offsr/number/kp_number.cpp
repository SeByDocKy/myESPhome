#include "kp_number.h"

namespace esphome {
namespace offsr {

void KpNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_kp(value);
}

}  // namespace offsr
}  // namespace esphome
