#include "output_max_number.h"

namespace esphome {
namespace offsr {

void OutputMaxNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_output_min(value);
}

}  // namespace offsr
}  // namespace esphome
