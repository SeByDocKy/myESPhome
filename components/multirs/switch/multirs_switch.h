#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SWITCH

#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"
#include "../multirs.h"

namespace esphome {
namespace multirs {

/// One on/off setting of register 0xD067 (see SwitchKind). The state is only published once the device confirmed it.
class MultiRSSwitch : public switch_::Switch, public Parented<MultiRS> {
 public:
  // int on purpose: cv.enum() emits integer literals in the generated code
  void set_kind(int kind) { this->kind_ = static_cast<SwitchKind>(kind); }
  SwitchKind get_kind() const { return this->kind_; }

 protected:
  void write_state(bool state) override { this->parent_->write_switch(this->kind_, state); }
  SwitchKind kind_{SWITCH_UPS_FUNCTION};
};

}  // namespace multirs
}  // namespace esphome

#endif  // USE_SWITCH
