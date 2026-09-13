#pragma once

#include "esphome/components/output/float_output.h"
#include "../hms.h"

namespace esphome {
namespace hms {

/// Standard ESPHome float output (0.0 - 1.0) driving the HMS's relative
/// power limit. Lets you control the inverter from any component
/// that produces a float output (PID, template, light, etc.).
class HMSPowerLimitPercentOutput : public output::FloatOutput {
 public:
  void set_parent(HMSComponent *parent) { this->parent_ = parent; }

 protected:
  void write_state(float state) override;
  HMSComponent *parent_{nullptr};
};

}  // namespace hms
}  // namespace esphome
