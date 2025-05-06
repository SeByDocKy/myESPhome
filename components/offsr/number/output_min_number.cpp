#include "output_min_number.h"

namespace esphome {
namespace offsr {

void OutputMinNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_output_min(value);
}

}  // namespace offsr
}  // namespace esphome
