#pragma once

#include "esphome/components/switch/switch.h"
#include "../minipid.h"

namespace esphome {
namespace minipid {

class ActivationSwitch : public switch_::Switch, public Component, public Parented<MINIPIDComponent> {
 public:
  void setup() override;

 protected:
  void write_state(bool state) override;
  ESPPreferenceObject pref_;
};

}  // namespace minipid
}  // namespace esphome
