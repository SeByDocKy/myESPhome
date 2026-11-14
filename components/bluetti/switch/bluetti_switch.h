#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../bluetti.h"

namespace esphome::bluetti {

/// Register that is 0 / 1 and can be written (AC output, grid charging, grid feed-in, system on/off): the state is
/// read back from the device, a command writes 1 / 0 (FC 06).
class BluettiSwitch : public switch_::Switch, public Component, public BluettiRegisterItem, public Parented<BluettiHub> {
 public:
  void dump_config() override;
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::bluetti
