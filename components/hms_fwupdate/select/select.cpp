// SPDX-License-Identifier: GPL-2.0-or-later
#include "select.h"

namespace esphome {
namespace hms_fwupdate {

void HMSFwUpdateFirmwareSelect::control(const std::string &value) {
  if (this->parent_ == nullptr) return;
  if (this->parent_->select_firmware(value)) this->publish_state(value);
}

}  // namespace hms_fwupdate
}  // namespace esphome
