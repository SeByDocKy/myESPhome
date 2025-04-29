#include "manual_override_switch.h"

namespace esphome {
namespace offsr {

void ManualOverrideSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_manual_override(state);
}

}  // namespace offsr
}  // namespace esphome
