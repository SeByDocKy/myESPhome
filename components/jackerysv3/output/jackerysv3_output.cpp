#include "jackerysv3_output.h"

#include <algorithm>
#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::jackerysv3 {

static const char *const TAG = "jackerysv3.output";
static constexpr uint32_t RESEND_MS = 30000;

void JackerySV3Output::write_state(float state) {
  if (std::isnan(state))
    return;
  state = std::min(std::max(state, 0.0f), 1.0f);

  float lo = this->min_, hi = this->max_;
  // The limits currently reported by the battery narrow the range
  float dev_lo, dev_hi;
  const JackeryState *st = this->parent_->state();
  if (st != nullptr && st->number_bounds(this->kind_, dev_lo, dev_hi) && dev_lo <= dev_hi) {
    lo = std::max(lo, dev_lo);
    hi = std::min(hi, dev_hi);
  }

  float value = lo + state * (hi - lo);
  if (this->step_ > 0.0f)
    value = std::round(value / this->step_) * this->step_;
  value = std::min(std::max(value, lo), hi);
  const int target = static_cast<int>(std::lround(value));

  const uint32_t now = millis();
  if (target == this->last_sent_ && now - this->last_sent_ms_ < RESEND_MS)
    return;
  if (this->parent_->send_main_command(number_json_field(this->kind_), target)) {
    ESP_LOGD(TAG, "%s <- %d (output %.3f)", number_json_field(this->kind_), target, state);
    this->last_sent_ = target;
    this->last_sent_ms_ = now;
  }
}

}  // namespace esphome::jackerysv3
