#include "ki_number.h"

namespace esphome {
namespace offsr {

void KiNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_ki(value);
}

}  // namespace offsr
}  // namespace esphome
