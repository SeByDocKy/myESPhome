#include "deyelvhy_number.h"

#include "esphome/core/defines.h"

#ifdef USE_NUMBER

namespace esphome::deyelvhy {

static const char *const TAG = "deyelvhy.number";

void DeyeLvHyNumber::dump_config() {
  LOG_NUMBER("", "Deye Number", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  scale: %g", this->reg_, static_cast<double>(this->scale_));
}

void DeyeLvHyNumber::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  const uint16_t word = this->word_(data);
  // Ignored while the register is held after a write (the poll may have been under way before the write)
  if (!this->parent_->accept_read(this->reg_, word))
    return;
  const float value = static_cast<float>(word) * this->scale_;
  if (this->has_state() && value == this->state)
    return;
  this->publish_state(value);
}

void DeyeLvHyNumber::control(float value) {
  const float raw = std::round(value / this->scale_);
  if (!(raw >= 0.0f && raw <= 65535.0f)) {
    ESP_LOGW(TAG, "Register %u: %g does not fit", this->reg(), static_cast<double>(value));
    return;
  }
  if (!this->parent_->write_register(this->reg_, static_cast<uint16_t>(raw))) {
    if (this->has_state())
      this->publish_state(this->state);  // put the number back to what the inverter has
    return;
  }
  this->publish_state(value);
}

}  // namespace esphome::deyelvhy

#endif  // USE_NUMBER
