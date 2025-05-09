#include "kd_number.h"

namespace esphome {
namespace offsr {

void KdNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_kd(value);
}

}  // namespace offsr
}  // namespace esphome
