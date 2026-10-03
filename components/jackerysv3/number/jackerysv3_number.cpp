#include "jackerysv3_number.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::jackerysv3 {

static const char *const TAG = "jackerysv3.number";

void JackerySV3Number::dump_config() {
  LOG_NUMBER("", "Jackery SolarVault 3 Number", this);
  ESP_LOGCONFIG(TAG, "  Field: %s", number_json_field(this->kind_));
}

void JackerySV3Number::control(float value) {
  if (std::isnan(value))
    return;
  const float clamped = std::min(std::max(value, this->traits.get_min_value()), this->traits.get_max_value());
  if (this->parent_->send_main_command(number_json_field(this->kind_), static_cast<int>(std::lround(clamped))))
    this->publish_state(clamped);  // optimistic; the next report from the battery has the last word
}

}  // namespace esphome::jackerysv3
