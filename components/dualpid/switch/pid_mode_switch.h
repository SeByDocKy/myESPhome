#pragma once

#include "esphome/components/switch/switch.h"
#include "../dualpid.h"

namespace esphome {
namespace dualpid {

class PidModeSwitch : public switch_::Switch, public Component, public Parented<OFFSRComponent> {
 public:
  void setup() override;

 protected:
  void write_state(bool state) override;
  ESPPreferenceObject pref_;
};

}  // namespace dualpid
}  // namespace esphome
