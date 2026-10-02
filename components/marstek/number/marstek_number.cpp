#include "marstek_number.h"

namespace esphome::marstek {

static const char *const TAG = "marstek.number";

void MarstekNumber::dump_config() {
  LOG_NUMBER("", "Marstek Number", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  scale: %g  skip_updates: %u", this->reg_, this->scale_, this->skip_updates);
}

void MarstekNumber::parse_and_publish(const std::vector<uint8_t> &data) {
  if (!this->has_data_(data))
    return;
  const float value = static_cast<float>(static_cast<double>(this->raw_(data)) * static_cast<double>(this->scale_));
  this->publish_state(value);
}

void MarstekNumber::control(float value) {
  const bool is_signed = this->sensor_value_type == SensorValueType::S_WORD;
  double raw = std::round(static_cast<double>(value) / static_cast<double>(this->scale_));
  raw = std::min(std::max(raw, is_signed ? -32768.0 : 0.0), is_signed ? 32767.0 : 65535.0);
  // Two's complement for the signed registers (schedule power: negative = charge)
  const uint16_t reg_value = static_cast<uint16_t>(static_cast<int32_t>(raw) & 0xFFFF);

  this->parent_->write_register(this->reg_, reg_value);
  this->publish_state(value);
}

}  // namespace esphome::marstek
