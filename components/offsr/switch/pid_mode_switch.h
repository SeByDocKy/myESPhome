#pragma once

#include "esphome/components/switch/switch.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class PidModeSwitch : public switch_::Switch, public Parented<OFFSRComponent> {
 public:
  PidModeSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace offsr
}  // namespace esphome
