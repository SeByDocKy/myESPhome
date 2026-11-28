#include "ecoflow_stream_binary_sensor.h"

namespace esphome::ecoflow_stream {

static const char *const TAG = "ecoflow_stream.binary_sensor";

void EcoflowStreamBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "EcoFlow STREAM Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u", this->reg_);
}

// Grid status (1 = on grid) and system master flag (1 = master)
void EcoflowStreamBinarySensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  this->publish_state(this->word_(data, 0) == 1);
}

}  // namespace esphome::ecoflow_stream
