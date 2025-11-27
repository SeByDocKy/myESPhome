#pragma once

#include "esphome/components/switch/switch.h"
#include "../bioffsr.h"

namespace esphome {
namespace bioffsr {

class Output2ActivationSwitch : public switch_::Switch, public Component, public Parented<BIOFFSRComponent> {
 public:
  void setup() override;

 protected:
  void write_state(bool state) override;
  ESPPreferenceObject pref_;
};

}  // namespace bioffsr
}  // namespace esphome


