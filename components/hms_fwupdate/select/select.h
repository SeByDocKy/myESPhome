// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "esphome/components/select/select.h"
#include "../hms_fwupdate.h"

namespace esphome {
namespace hms_fwupdate {

/// Chooses which entry of the `firmwares:` list `start` will download.
class HMSFwUpdateFirmwareSelect : public select::Select {
 public:
  void set_parent(HMSFwUpdateComponent *parent) { this->parent_ = parent; }

 protected:
  void control(const std::string &value) override;
  HMSFwUpdateComponent *parent_{nullptr};
};

}  // namespace hms_fwupdate
}  // namespace esphome
