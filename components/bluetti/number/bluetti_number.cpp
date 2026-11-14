#include "bluetti_number.h"

namespace esphome::bluetti {

static const char *const TAG = "bluetti.number";

void BluettiNumber::dump_config() {
  LOG_NUMBER("", "BLUETTI Number", this);
  ESP_LOGCONFIG(TAG, "  Register: %u", this->reg_);
}

void BluettiNumber::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data) || this->held_())
    return;
  this->publish_state(static_cast<float>(this->word_(data, 0)));
}

void BluettiNumber::control(float value) {
  const float limited = std::min(std::max(value, this->traits.get_min_value()), this->traits.get_max_value());
  const uint16_t reg_value = static_cast<uint16_t>(std::lround(limited));
  this->parent_->write_register(this->reg_, reg_value);
  this->hold_for(WRITE_HOLD_MS);
  this->publish_state(static_cast<float>(reg_value));
}

}  // namespace esphome::bluetti
