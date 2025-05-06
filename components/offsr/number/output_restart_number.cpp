#include "output_restart_number.h"

namespace esphome {
namespace offsr {

void OutputRestartNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_output_restart(value);
}

}  // namespace offsr
}  // namespace esphome
