#pragma once

#include "esphome/components/switch/switch.h"
#include "../offsr.h"

namespace esphome {
namespace osr {

class ActivationSwitch : public switch_::Switch, public Parented<OFFSRComponent> {
 public:
  ActivationSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace offsr
}  // namespace esphome
