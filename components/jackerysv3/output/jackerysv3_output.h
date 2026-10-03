#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"
#include "../jackerysv3.h"

namespace esphome::jackerysv3 {

/// Float output (0.0 .. 1.0) driving a writable setting of the battery: 0.0 maps to the minimum of the setting,
/// 1.0 to its maximum (maximum grid output power 0 .. 2500 W, SOC charge limit 50 .. 100 %, SOC discharge limit
/// 5 .. 49 %). Meant for control loops (PID, zero injection ...): the command is only sent when the integer value
/// changes, and repeated every 30 s otherwise in case the battery missed it.
class JackerySV3Output : public output::FloatOutput, public Parented<JackerySV3Hub> {
 public:
  void set_kind(NumberKind kind) { this->kind_ = kind; }
  void set_range(float min_value, float max_value, float step) {
    this->min_ = min_value;
    this->max_ = max_value;
    this->step_ = step;
  }

 protected:
  void write_state(float state) override;

  NumberKind kind_{NumberKind::MAX_OUTPUT_POWER};
  float min_{0.0f}, max_{1.0f}, step_{1.0f};
  int last_sent_{-1};
  uint32_t last_sent_ms_{0};
};

}  // namespace esphome::jackerysv3
