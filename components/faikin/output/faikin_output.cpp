#include "faikin_output.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome {
namespace faikin {

static const char *const TAG = "faikin.output";

void FaikinOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Faikin demand output:\n"
                "  0.0 = %d %%, 1.0 = %d %%, step %d %%",
                this->min_demand_, this->max_demand_, this->step_);
}

int FaikinOutput::level_to_percent(float level) const {
  if (std::isnan(level) || level < 0.0f)
    level = 0.0f;
  if (level > 1.0f)
    level = 1.0f;
  const float raw = this->min_demand_ + level * static_cast<float>(this->max_demand_ - this->min_demand_);
  // Round to the nearest multiple of the step, counted from the minimum.
  int percent = this->min_demand_ + static_cast<int>(lroundf((raw - this->min_demand_) / this->step_)) * this->step_;
  if (percent > this->max_demand_)
    percent = this->max_demand_;
  if (percent < this->min_demand_)
    percent = this->min_demand_;
  return percent;
}

void FaikinOutput::write_state(float state) {
  const int percent = this->level_to_percent(state);
  // A PID typically writes every cycle: only send a command when the target percentage changes.
  if (percent == this->last_sent_)
    return;
  if (this->parent_->control_demand(percent))
    this->last_sent_ = percent;
}

}  // namespace faikin
}  // namespace esphome
