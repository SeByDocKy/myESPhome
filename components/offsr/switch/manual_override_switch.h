#pragma once

#include "esphome/components/switch/switch.h"
#include "../offsr.h"

namespace esphome {
namespace osr {

class ManualOverrideSwitch : public switch_::Switch, public Parented<OFFSRComponent> {
 public:
  ManualOverrideSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace offsr
}  // namespace esphome
