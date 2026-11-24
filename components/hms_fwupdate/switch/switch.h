// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "esphome/components/switch/switch.h"
#include "../hms_fwupdate.h"

namespace esphome {
namespace hms_fwupdate {

/// Safety interlock of the update: `start` only works while this switch is on.
class HMSFwUpdateArmedSwitch : public switch_::Switch {
 public:
  void set_parent(HMSFwUpdateComponent *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;
  HMSFwUpdateComponent *parent_{nullptr};
};

}  // namespace hms_fwupdate
}  // namespace esphome
