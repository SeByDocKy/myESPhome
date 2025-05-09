#include "activation_switch.h"

namespace esphome {
namespace offsr {

void ActivationSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_activation(state);
}

}  // namespace offsr
}  // namespace esphome
