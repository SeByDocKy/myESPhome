#pragma once

#include "esphome/components/switch/switch.h"
#include "../dualpid.h"

namespace esphome::dualpid {

class AllowChargingSwitch : public switch_::Switch, public Component, public Parented<DUALPIDComponent> {
 public:
  void setup() override;

 protected:
  void write_state(bool state) override;
  ESPPreferenceObject pref_;
};

}  // namespace esphome
