#include "bluetti_switch.h"

namespace esphome::bluetti {

static const char *const TAG = "bluetti.switch";

void BluettiSwitch::dump_config() {
  LOG_SWITCH("", "BLUETTI Switch", this);
  ESP_LOGCONFIG(TAG, "  Register: %u", this->reg_);
}

void BluettiSwitch::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data) || this->held_())
    return;
  this->publish_state(this->word_(data, 0) != 0);
}

void BluettiSwitch::write_state(bool state) {
  this->parent_->write_register(this->reg_, state ? 1 : 0);
  this->hold_for(WRITE_HOLD_MS);
  this->publish_state(state);
}

}  // namespace esphome::bluetti
