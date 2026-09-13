#pragma once

#include "esphome/components/output/float_output.h"
#include "../hm.h"

namespace esphome {
namespace hm {

/// Standard ESPHome float output (0.0 - 1.0) driving the HM's relative
/// power limit. Lets you control the inverter from any component
/// that produces a float output (PID, template, light, etc.).
class HMPowerLimitPercentOutput : public output::FloatOutput {
 public:
  void set_parent(HMComponent *parent) { this->parent_ = parent; }

 protected:
  void write_state(float state) override;
  HMComponent *parent_{nullptr};
};

}  // namespace hm
}  // namespace esphome
