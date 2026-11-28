#include "ecoflow_stream_sensor.h"

namespace esphome::ecoflow_stream {

static const char *const TAG = "ecoflow_stream.sensor";

void EcoflowStreamSensor::dump_config() {
  LOG_SENSOR("", "EcoFlow STREAM Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  count: %u", this->reg_, this->count_);
}

void EcoflowStreamSensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  this->publish_state(this->number_(data));
}

}  // namespace esphome::ecoflow_stream
