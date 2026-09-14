#include "sensor_combined.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace sensor_combined {

static const char *const TAG = "sensor_combined";

void SensorCombined::setup() {
  // Recompute every time any source sensor reports a new state.
  for (auto *s : this->sensors_) {
    s->add_on_state_callback([this](float state) { this->update_(); });
  }
}

void SensorCombined::dump_config() {
  ESP_LOGCONFIG(TAG, "Sensor Combined:");
  const char *op_name = "sum";
  if (this->operation_ == COMBINE_OP_MEAN)
    op_name = "mean";
  else if (this->operation_ == COMBINE_OP_PROD)
    op_name = "prod";
  ESP_LOGCONFIG(TAG, "  Operation: %s", op_name);
  ESP_LOGCONFIG(TAG, "  Propagate NaN: %s", YESNO(this->propagate_nan_));
  ESP_LOGCONFIG(TAG, "  Number of source sensors: %u", this->sensors_.size());
  LOG_SENSOR("  ", "Sensor Combined", this);
}

void SensorCombined::update_() {
  // Wait until every source sensor has published at least one state,
  // regardless of propagate_nan (has_state() is about "ever
  // published", not about the value being NaN).
  for (auto *s : this->sensors_) {
    if (!s->has_state())
      return;
  }

  float accumulator = (this->operation_ == COMBINE_OP_PROD) ? 1.0f : 0.0f;
  uint32_t valid_count = 0;
  bool has_nan = false;

  for (auto *s : this->sensors_) {
    float v = s->state;
    if (std::isnan(v)) {
      has_nan = true;
      if (this->propagate_nan_)
        break;  // no need to keep scanning, the result will be NaN anyway
      continue;   // skip this sensor, keep combining the others
    }
    valid_count++;
    if (this->operation_ == COMBINE_OP_PROD) {
      accumulator *= v;
    } else {
      accumulator += v;
    }
  }

  if (has_nan && this->propagate_nan_) {
    this->publish_state(NAN);
    return;
  }

  if (valid_count == 0) {
    // Every source sensor is currently NaN: nothing valid to combine.
    this->publish_state(NAN);
    return;
  }

  if (this->operation_ == COMBINE_OP_MEAN)
    accumulator /= valid_count;

  this->publish_state(accumulator);
}

}  // namespace sensor_combined
}  // namespace esphome
