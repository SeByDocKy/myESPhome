#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/output/float_output.h"
#include "../ecoflow_stream.h"

namespace esphome::ecoflow_stream {

/// 0.0 .. 1.0 -> 0 .. maximum charge power. The request sent is (discharge - charge), so a PID loop can drive each
/// output independently.
class EcoflowStreamChargeOutput : public output::FloatOutput, public Parented<EcoflowStreamHub> {
 protected:
  void write_state(float state) override { this->parent_->set_power_fraction(true, state); }
};

/// 0.0 .. 1.0 -> 0 .. maximum discharge power
class EcoflowStreamDischargeOutput : public output::FloatOutput, public Parented<EcoflowStreamHub> {
 protected:
  void write_state(float state) override { this->parent_->set_power_fraction(false, state); }
};

}  // namespace esphome::ecoflow_stream
