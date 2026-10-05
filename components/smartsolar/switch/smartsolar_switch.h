#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SWITCH

#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"
#include "../smartsolar.h"

namespace esphome {
namespace smartsolar {

/// One writable on/off setting of the MPPT (see SwitchKind). The state is only published once the charger has
/// confirmed the change.
class SmartSolarSwitch : public switch_::Switch, public Parented<SmartSolar> {
 public:
  // int on purpose: cv.enum() emits integer literals in the generated code
  void set_kind(int kind) { this->kind_ = static_cast<SwitchKind>(kind); }
  SwitchKind get_kind() const { return this->kind_; }

 protected:
  void write_state(bool state) override { this->parent_->write_switch(this->kind_, state); }

  SwitchKind kind_{SWITCH_CHARGER};
};

}  // namespace smartsolar
}  // namespace esphome

#endif  // USE_SWITCH
