// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "esphome/components/button/button.h"
#include "../hms_fwupdate.h"

namespace esphome {
namespace hms_fwupdate {

/// On press: verify the selected firmware and, unless dry_run, start the update.
class HMSFwUpdateStartButton : public button::Button {
 public:
  void set_parent(HMSFwUpdateComponent *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  HMSFwUpdateComponent *parent_{nullptr};
};

/// On press: stop the running update (the inverter may stay in its bootloader until an update succeeds).
class HMSFwUpdateAbortButton : public button::Button {
 public:
  void set_parent(HMSFwUpdateComponent *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  HMSFwUpdateComponent *parent_{nullptr};
};

}  // namespace hms_fwupdate
}  // namespace esphome
