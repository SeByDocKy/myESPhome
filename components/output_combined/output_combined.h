#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include <vector>

namespace esphome {
namespace output_combined {

// A FloatOutput that fans out a single state to any number of
// other FloatOutput instances. Equivalent to a "template" output
// whose write_action calls output.set_level on several outputs,
// but validated at compile time and without the lambda/action overhead.
class OutputCombined : public output::FloatOutput, public Component {
 public:
  // Called once per entry in the YAML "outputs" list (see __init__.py).
  void add_output(output::FloatOutput *out) { this->outputs_.push_back(out); }

  void dump_config() override;

 protected:
  // Forwards the level to every registered sub-output.
  // Each sub-output still applies its own min_power/max_power/
  // zero_means_zero clamping via set_level().
  void write_state(float state) override {
    for (auto *out : this->outputs_)
      out->set_level(state);
  }

  std::vector<output::FloatOutput *> outputs_;
};

}  // namespace output_combined
}  // namespace esphome
