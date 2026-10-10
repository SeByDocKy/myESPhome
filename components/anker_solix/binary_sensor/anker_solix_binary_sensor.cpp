#include "anker_solix_binary_sensor.h"

namespace esphome::anker_solix {

static const char *const TAG = "anker_solix.binary_sensor";

void AnkerSolixBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Anker SOLIX Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u", this->reg_);
}

void AnkerSolixBinarySensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data) || this->held_())
    return;
  this->publish_state(this->word_(data, 0) != 0);
}

}  // namespace esphome::anker_solix
