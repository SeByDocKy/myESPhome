#pragma once

#include "esphome/components/switch/switch.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class ReverseSwitch : public switch_::Switch, public Component, public Parented<MINIPIDComponent> {
 public:
  void setup() override;

 protected:
  void write_state(bool state) override;
  ESPPreferenceObject pref_;
};

}  // namespace minipid

}  // namespace esphome

