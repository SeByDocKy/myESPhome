#include "pid_mode_switch.h"

namespace esphome {
namespace offsr {

void PidModeSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_pid_mode(state);
}

}  // namespace offsr
}  // namespace esphome
