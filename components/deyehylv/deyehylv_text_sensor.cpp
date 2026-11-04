#include "deyehylv_text_sensor.h"

#include "esphome/core/defines.h"

#ifdef USE_TEXT_SENSOR

#include <cstdio>

namespace esphome::deyehylv {

static const char *const TAG = "deyehylv.text_sensor";

static void publish_if_changed(text_sensor::TextSensor *sensor, const char *text) {
  if (sensor->has_state() && sensor->state == text)
    return;
  sensor->publish_state(text);
}

void DeyeHyLvStateTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Deye Overall State", this);
  ESP_LOGCONFIG(TAG, "  Register: %u", this->reg_);
}

void DeyeHyLvStateTextSensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  const char *text;
  switch (this->word_(data)) {
    case 0:
      text = "standby";
      break;
    case 1:
      text = "selftest";
      break;
    case 2:
      text = "normal";
      break;
    case 3:
      text = "alarm";
      break;
    case 4:
      text = "fault";
      break;
    default:
      text = "unknown";
      break;
  }
  publish_if_changed(this, text);
}

void DeyeHyLvTimeTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "Deye Time Slot", this);
  ESP_LOGCONFIG(TAG, "  Register: %u", this->reg_);
}

void DeyeHyLvTimeTextSensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  int32_t value = this->word_(data);
  if (value > 32767)  // same reading as the time-of-use start time sensors
    value -= 65535;
  if (value < 0)
    value = 0;
  char text[8];
  snprintf(text, sizeof(text), "%02u:%02u", static_cast<unsigned>(value / 100) % 100,
           static_cast<unsigned>(value % 100));
  publish_if_changed(this, text);
}

}  // namespace esphome::deyehylv

#endif  // USE_TEXT_SENSOR
