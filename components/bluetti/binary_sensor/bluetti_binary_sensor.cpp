#include "bluetti_binary_sensor.h"

namespace esphome::bluetti {

static const char *const TAG = "bluetti.binary_sensor";

void BluettiBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "BLUETTI Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u", this->reg_);
}

void BluettiBinarySensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  this->publish_state(this->word_(data, 0) != 0);
}

}  // namespace esphome::bluetti
