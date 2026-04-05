#pragma once

#include "esphome/components/switch/switch.h"
#include "../dualpidpcm.h"

namespace esphome {
namespace dualpidpcm {

class ManualOverrideSwitch : public switch_::Switch, public Component, public Parented<DUALPIDPCMComponent> {
 public:
  void setup() override;

 protected:
  void write_state(bool state) override;
  ESPPreferenceObject pref_;
};

}  // namespace dualpidpcm
}  // namespace esphome
