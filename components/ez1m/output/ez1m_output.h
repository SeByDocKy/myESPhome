#pragma once

#include "esphome/components/output/float_output.h"
#include "../ez1m.h"

namespace esphome {
namespace ez1m {

// Maps a standard ESPHome FloatOutput (0.0 - 1.0) onto the EZ1-M's power
// limit command: 1.0 == max_power_ watts (800 W by default, the EZ1-M's
// hardware ceiling), 0.0 turns the inverter off outright.
class EZ1MOutput : public output::FloatOutput, public Parented<EZ1MComponent> {
 public:
  void set_max_power(float watts) { this->max_power_ = watts; }

 protected:
  void write_state(float state) override;

  float max_power_{800.0f};
};

}  // namespace ez1m
}  // namespace esphome
