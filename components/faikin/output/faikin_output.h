#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/faikin/faikin.h"

namespace esphome {
namespace faikin {

/// Float output driving the demand control limit: 0.0 maps to the minimum power (40 % by default) and
/// 1.0 maps to the maximum power (100 % by default). Meant to be driven by a PID or any float source.
class FaikinOutput : public output::FloatOutput, public Component, public Parented<FaikinComponent> {
 public:
  void dump_config() override;

  void set_min_demand(int percent) { this->min_demand_ = percent; }
  void set_max_demand(int percent) { this->max_demand_ = percent; }
  void set_step(int percent) { this->step_ = percent; }

  /// Converts an output level in [0, 1] to a demand percentage (rounded to the step, clamped).
  int level_to_percent(float level) const;

 protected:
  void write_state(float state) override;

  int min_demand_{40};
  int max_demand_{100};
  int step_{5};
  int last_sent_{-1};
};

}  // namespace faikin
}  // namespace esphome
