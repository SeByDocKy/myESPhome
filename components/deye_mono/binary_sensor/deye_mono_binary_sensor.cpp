#include "deye_mono_binary_sensor.h"

namespace esphome::deye_mono {

static const char *const TAG = "deye_mono.binary_sensor";

void DeyeMonoBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Deye Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  mask: 0x%04X", this->reg_, this->mask_);
}

void DeyeMonoBinarySensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  this->publish_state((this->word_(data) & this->mask_) != 0);
}

}  // namespace esphome::deye_mono
