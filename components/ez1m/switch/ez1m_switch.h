#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../ez1m.h"

namespace esphome {
namespace ez1m {

class EZ1MSwitch : public switch_::Switch, public Component, public Parented<EZ1MComponent> {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace ez1m
}  // namespace esphome
