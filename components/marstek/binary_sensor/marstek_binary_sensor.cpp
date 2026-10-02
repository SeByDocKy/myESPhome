#include "marstek_binary_sensor.h"

namespace esphome::marstek {

static const char *const TAG = "marstek.binary_sensor";

void MarstekBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Marstek Binary Sensor", this);
  if (this->register_count > 0) {
    ESP_LOGCONFIG(TAG, "  Register: %u  count: %u  skip_updates: %u", this->reg_, this->register_count,
                  this->skip_updates);
  }
}

void MarstekBinarySensor::parse_and_publish(const std::vector<uint8_t> &data) {
  if (!this->has_data_(data))
    return;

  bool state = false;
  if (this->kind_ == BinaryKind::BITS_ANY) {
    for (size_t i = 0; i < this->register_count; i++) {
      if (this->word_(data, i) != 0)
        state = true;
    }
  } else {
    state = this->word_(data, 0) != 0;
  }
  this->publish_state(state);
}

}  // namespace esphome::marstek
