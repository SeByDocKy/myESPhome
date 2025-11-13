#pragma once

#include "esphome/components/switch/switch.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class ReverseSwitch : public switch_::Switch, public Component, public Parented<OFFSRComponent> {
 public:
  void setup() override;

 protected:
  void write_state(bool state) override;
  ESPPreferenceObject pref_;
};

}  // namespace offsr

}  // namespace esphome



