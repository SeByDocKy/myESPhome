#include "ecoflow_stream_text_sensor.h"

#include <cstdio>

namespace esphome::ecoflow_stream {

static const char *const TAG = "ecoflow_stream.text_sensor";

void EcoflowStreamTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "EcoFlow STREAM Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  count: %u", this->reg_, this->count_);
}

void EcoflowStreamTextSensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  char buffer[24];
  std::string text;
  switch (this->kind_) {
    case TextKind::STRING:
      for (size_t i = 0; i < this->count_; i++) {
        const uint16_t word = this->word_(data, i);
        const char chars[2] = {static_cast<char>(word >> 8), static_cast<char>(word & 0xFF)};
        for (const char c : chars) {
          if (c >= 0x20 && c <= 0x7E)
            text.push_back(c);
        }
      }
      while (!text.empty() && text.back() == ' ')
        text.pop_back();
      break;
    case TextKind::VERSION: {
      const uint32_t value = static_cast<uint32_t>(this->raw_(data));
      snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", static_cast<unsigned>(value >> 24),
               static_cast<unsigned>((value >> 16) & 0xFF), static_cast<unsigned>((value >> 8) & 0xFF),
               static_cast<unsigned>(value & 0xFF));
      text = buffer;
      break;
    }
    case TextKind::DECIMAL:
      snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned>(this->raw_(data)));
      text = buffer;
      break;
    case TextKind::STATES: {
      const uint16_t value = this->word_(data, 0);
      for (const auto &state : this->states_) {
        if (state.first == value)
          text = state.second;
      }
      if (text.empty()) {
        snprintf(buffer, sizeof(buffer), "Unknown (%u)", static_cast<unsigned>(value));
        text = buffer;
      }
      break;
    }
    case TextKind::CODES:
      for (size_t i = 0; i < this->count_; i++) {
        const uint16_t code = this->word_(data, i);
        if (code == 0)
          continue;
        snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned>(code));
        if (!text.empty())
          text += ", ";
        text += buffer;
      }
      if (text.empty())
        text = "None";
      break;
  }
  if (this->published_ && text == this->last_)
    return;
  this->last_ = text;
  this->published_ = true;
  this->publish_state(text);
}

}  // namespace esphome::ecoflow_stream
