#include "deyelvhy_binary_sensor.h"

#include "esphome/core/defines.h"

#ifdef USE_BINARY_SENSOR

namespace esphome::deyelvhy {

static const char *const TAG = "deyelvhy.binary_sensor";

void DeyeLvHyBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Deye Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  mask: 0x%04X", this->reg_, this->mask_);
}

void DeyeLvHyBinarySensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  this->publish_state((this->word_(data) & this->mask_) != 0);
}

}  // namespace esphome::deyelvhy

#endif  // USE_BINARY_SENSOR
