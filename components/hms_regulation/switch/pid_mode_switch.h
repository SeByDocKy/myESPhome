#pragma once

#include "esphome/components/switch/switch.h"
#include "../hms_regulation.h"

namespace esphome {
namespace hms_regulation {

class PidModeSwitch : public switch_::Switch, public Component, public Parented<HMS_REGULATIONComponent> {
 public:
  // PidModeSwitch() = default;
  void setup() override;

 protected:
  void write_state(bool state) override;
  ESPPreferenceObject pref_;
};

}  // namespace hms_regulation
}  // namespace esphome
