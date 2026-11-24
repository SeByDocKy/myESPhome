// SPDX-License-Identifier: GPL-2.0-or-later
#include "switch.h"

namespace esphome {
namespace hms_fwupdate {

void HMSFwUpdateArmedSwitch::write_state(bool state) {
  if (this->parent_ == nullptr) return;
  this->parent_->set_armed(state);  // publishes the switch state itself
}

}  // namespace hms_fwupdate
}  // namespace esphome
