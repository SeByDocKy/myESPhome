#pragma once

#include "esphome/components/output/float_output.h"
#include "../pcm3k6w.h"

namespace esphome::pcm3k6w {

// Mirrors the original YAML's `output: platform: template` entries: a 0.0-1.0
// float level (e.g. from a `fan: platform: speed`) is linearly mapped to
// [min_value_, max_value_] amps and written to the same volatile current
// register as the matching number entity. output::FloatOutput has no
// Component base and no state of its own - it's a stateless pass-through.
class PCM3K6WOutput : public output::FloatOutput, public Parented<PCM3K6WComponent> {
 public:
  void set_kind(uint8_t kind) { this->kind_ = kind; }
  void set_range(float min_value, float max_value) {
    this->min_value_ = min_value;
    this->max_value_ = max_value;
  }

 protected:
  void write_state(float state) override;

  uint8_t kind_{0};
  float min_value_{0.0f};
  float max_value_{1.0f};
};

}  // namespace esphome::pcm3k6w
