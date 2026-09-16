#include "ez1m_output.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome {
namespace ez1m {

static const char *const TAG = "ez1m.output";

void EZ1MOutput::write_state(float state) {
  // `state` arrives already normalized/clamped to [0.0, 1.0] by the
  // output::FloatOutput base class (min_power/max_power/power_supply, etc.
  // are handled upstream if configured).
  float watts = state * this->max_power_;

  if (watts <= 0.0f) {
    this->parent_->turn_off();
    return;
  }

  // No lower clamp here: the caller (e.g. a PID climate/regulation
  // component upstream) is expected to enforce its own output_min, since
  // the right floor depends on that regulation loop, not on this output.
  watts = std::min(this->max_power_, watts);
  this->parent_->set_power_limit(watts);
}

}  // namespace ez1m
}  // namespace esphome
