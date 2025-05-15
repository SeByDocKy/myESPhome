#pragma once

#include "esphome/components/switch/switch.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class ManualOverrideSwitch : public switch_::Switch, public Component, public Parented<OFFSRComponent> {
 public:
  // ManualOverrideSwitch() = default;
  void setup() override;

 protected:
  void write_state(bool state) override;
  ESPPreferenceObject pref_;
};

}  // namespace offsr
}  // namespace esphome
