#include "anker_solix_text_sensor.h"

namespace esphome::anker_solix {

static const char *const TAG = "anker_solix.text_sensor";

void AnkerSolixTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Anker SOLIX Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  count: %u", this->reg_, this->count_);
}

void AnkerSolixTextSensor::publish_if_changed_(const std::string &value) {
  if (this->published_ && value == this->last_)
    return;
  this->last_ = value;
  this->published_ = true;
  this->publish_state(value);
}

// The registers hold the characters in register order (high byte first), cut at the first NUL, printable ASCII only
std::string AnkerSolixTextSensor::decode_text_(std::span<const uint8_t> data) const {
  std::string text;
  const size_t length = static_cast<size_t>(this->count_) * 2u;
  text.reserve(length);
  for (size_t i = 0; i < length; i++) {
    const char c = static_cast<char>(data[this->offset + i]);
    if (c == '\0')
      break;
    if (c >= 0x20 && c <= 0x7E)
      text.push_back(c);
  }
  while (!text.empty() && text.back() == ' ')
    text.pop_back();
  return text;
}

void AnkerSolixTextSensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;

  switch (this->kind_) {
    case TextKind::CHAR:
      this->publish_if_changed_(this->decode_text_(data));
      break;

    case TextKind::STATES: {
      const uint16_t value = this->word_(data, 0);
      for (const auto &state : this->states_) {
        if (state.first == value) {
          this->publish_if_changed_(state.second);
          return;
        }
      }
      char buf[24];
      snprintf(buf, sizeof(buf), "Unknown (%u)", static_cast<unsigned>(value));
      this->publish_if_changed_(buf);
      break;
    }

    case TextKind::PRODUCT: {
      // 16 character serial number: characters 4..6 are the product code, 17 character one: characters 4..7
      const std::string sn = this->decode_text_(data);
      std::string code;
      if (sn.size() == 16)
        code = sn.substr(3, 3);
      else if (sn.size() >= 17)
        code = sn.substr(3, 4);
      if (code.empty()) {
        this->publish_if_changed_(this->default_name_);
        return;
      }
      for (const auto &product : this->products_) {
        if (product.first == code) {
          this->publish_if_changed_(product.second);
          return;
        }
      }
      if (!this->warned_) {
        this->warned_ = true;
        ESP_LOGW(TAG, "Product code '%s' (serial number %s) is not known for the %s register map: using '%s'",
                 code.c_str(), sn.c_str(), this->model_name_.c_str(), this->default_name_.c_str());
      }
      this->publish_if_changed_(this->default_name_);
      break;
    }
  }
}

}  // namespace esphome::anker_solix
