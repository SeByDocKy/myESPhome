#include "marstek_switch.h"

namespace esphome::marstek {

static const char *const TAG = "marstek.switch";

void MarstekSwitch::dump_config() {
  LOG_SWITCH("", "Marstek Switch", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  on: %u  off: %u  skip_updates: %u", this->reg_, this->on_value_,
                this->off_value_, this->skip_updates);
}

void MarstekSwitch::parse_and_publish(const std::vector<uint8_t> &data) {
  if (!this->has_data_(data))
    return;
  const uint16_t value = this->word_(data, 0);
  if (this->reg_ == REG_RS485_CONTROL_MODE && this->parent_ != nullptr)
    this->parent_->set_rs485_enabled(value == RS485_CONTROL_ENABLE);
  this->publish_state(value == this->on_value_);
}

void MarstekSwitch::write_state(bool state) {
  this->parent_->write_register(this->reg_, state ? this->on_value_ : this->off_value_);
  this->publish_state(state);
}

}  // namespace esphome::marstek
