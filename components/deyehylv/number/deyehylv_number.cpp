#include "deyehylv_number.h"

#include "esphome/core/defines.h"

#ifdef USE_NUMBER

namespace esphome::deyehylv {

static const char *const TAG = "deyehylv.number";

void DeyeHyLvNumber::dump_config() {
  LOG_NUMBER("", "Deye Number", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  scale: %g", this->reg_, static_cast<double>(this->scale_));
}

void DeyeHyLvNumber::parse_and_publish(std::span<const uint8_t> data) {
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

void DeyeHyLvNumber::control(float value) {
  const float raw = std::round(value / this->scale_);
  if (!(raw >= 0.0f && raw <= 65535.0f)) {
    ESP_LOGW(TAG, "'%s' = %g does not fit the register", this->get_name().c_str(), static_cast<double>(value));
    return;
  }
  if (!this->parent_->write_register(this->reg_, static_cast<uint16_t>(raw))) {
    if (this->has_state())
      this->publish_state(this->state);  // put the number back to what the inverter has
    return;
  }
  this->publish_state(value);
}

}  // namespace esphome::deyehylv

#endif  // USE_NUMBER
