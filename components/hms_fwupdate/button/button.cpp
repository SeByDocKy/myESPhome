// SPDX-License-Identifier: GPL-2.0-or-later
#include "button.h"

namespace esphome {
namespace hms_fwupdate {

void HMSFwUpdateStartButton::press_action() {
  if (this->parent_ != nullptr) this->parent_->start();
}

void HMSFwUpdateAbortButton::press_action() {
  if (this->parent_ != nullptr) this->parent_->abort();
}

}  // namespace hms_fwupdate
}  // namespace esphome
