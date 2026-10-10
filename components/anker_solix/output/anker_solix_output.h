#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/output/float_output.h"
#include "../anker_solix.h"

namespace esphome::anker_solix {

/// 0.0 .. 1.0 -> 0 .. maximum charge power of the battery. The request sent is (discharge - charge), so a PID loop
/// can drive each output independently.
class AnkerSolixChargeOutput : public output::FloatOutput, public Parented<AnkerSolixHub> {
 protected:
  void write_state(float state) override { this->parent_->set_power_fraction(true, state); }
};

/// 0.0 .. 1.0 -> 0 .. maximum discharge power of the battery
class AnkerSolixDischargeOutput : public output::FloatOutput, public Parented<AnkerSolixHub> {
 protected:
  void write_state(float state) override { this->parent_->set_power_fraction(false, state); }
};

}  // namespace esphome::anker_solix
