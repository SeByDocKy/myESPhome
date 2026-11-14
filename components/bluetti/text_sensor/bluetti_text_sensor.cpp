#include "bluetti_text_sensor.h"

namespace esphome::bluetti {

static const char *const TAG = "bluetti.text_sensor";

void BluettiTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "BLUETTI Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  count: %u", this->reg_, this->count_);
}

void BluettiTextSensor::publish_if_changed_(const std::string &value) {
  if (this->published_ && value == this->last_)
    return;
  this->last_ = value;
  this->published_ = true;
  this->publish_state(value);
}

void BluettiTextSensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  char buffer[24];
  std::string text;
  switch (this->kind_) {
    case TextKind::STRING: {
      // The first character of each register is in its low byte, the second in its high byte; cut at a NUL
      for (size_t i = 0; i < this->count_; i++) {
        const uint16_t word = this->word_(data, i);
        const char chars[2] = {static_cast<char>(word & 0xFF), static_cast<char>(word >> 8)};
        for (const char c : chars) {
          if (c >= 0x20 && c <= 0x7E)
            text.push_back(c);
        }
      }
      while (!text.empty() && text.back() == ' ')
        text.pop_back();
      break;
    }
    case TextKind::SERIAL: {
      uint64_t value = 0;
      for (size_t i = 0; i < this->count_ && i < 4; i++)
        value |= static_cast<uint64_t>(this->word_(data, i)) << (16u * i);
      snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
      text = buffer;
      break;
    }
    case TextKind::VERSION: {
      const uint32_t value = static_cast<uint32_t>(this->word_(data, 0)) | (static_cast<uint32_t>(this->word_(data, 1)) << 16);
      snprintf(buffer, sizeof(buffer), "%u.%02u.%02u", static_cast<unsigned>(value / 10000u),
               static_cast<unsigned>((value / 100u) % 100u), static_cast<unsigned>(value % 100u));
      text = buffer;
      break;
    }
    case TextKind::STATES: {
      const uint16_t value = this->word_(data, 0);
      for (const auto &state : this->states_) {
        if (state.first == value) {
          text = state.second;
          break;
        }
      }
      if (text.empty()) {
        snprintf(buffer, sizeof(buffer), "Unknown (%u)", static_cast<unsigned>(value));
        text = buffer;
      }
      break;
    }
    case TextKind::FLAGS: {
      for (size_t i = 0; i < this->count_; i++) {
        const uint16_t word = this->word_(data, i);
        for (uint8_t bit = 0; bit < 16; bit++) {
          if (((word >> bit) & 1u) == 0)
            continue;
          const uint8_t number = static_cast<uint8_t>(16u * i + bit);
          std::string label;
          for (const auto &flag : this->flags_) {
            if (flag.first == number) {
              label = flag.second;
              break;
            }
          }
          if (label.empty()) {
            snprintf(buffer, sizeof(buffer), "bit %u", static_cast<unsigned>(number));
            label = buffer;
          }
          if (!text.empty())
            text += ", ";
          text += label;
        }
      }
      if (text.empty())
        text = "None";
      break;
    }
    case TextKind::HEX: {
      // Low word first on the bus: the highest register is printed first
      text = "0x";
      for (size_t i = this->count_; i > 0; i--) {
        snprintf(buffer, sizeof(buffer), "%04X", static_cast<unsigned>(this->word_(data, i - 1)));
        text += buffer;
      }
      break;
    }
  }
  this->publish_if_changed_(text);
}

}  // namespace esphome::bluetti
