#include "bluetti_select.h"

namespace esphome::bluetti {

static const char *const TAG = "bluetti.select";

void BluettiSelect::dump_config() {
  LOG_SELECT("", "BLUETTI Select", this);
  ESP_LOGCONFIG(TAG, "  Register: %u", this->reg_);
}

void BluettiSelect::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data) || this->held_())
    return;
  const uint16_t value = this->word_(data, 0);
  for (size_t i = 0; i < this->values_.size(); i++) {
    if (this->values_[i] == value) {
      this->publish_state(i);
      return;
    }
  }
  ESP_LOGD(TAG, "Register %u holds %u, which is not one of the options of '%s'", this->reg_, value,
           this->get_name().c_str());
}

void BluettiSelect::control(size_t index) {
  if (index >= this->values_.size())
    return;
  this->parent_->write_register(this->reg_, this->values_[index]);
  this->hold_for(WRITE_HOLD_MS);
  this->publish_state(index);
}

}  // namespace esphome::bluetti
