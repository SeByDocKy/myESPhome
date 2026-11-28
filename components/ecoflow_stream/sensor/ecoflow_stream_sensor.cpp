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
  const float value = this->number_(data);
  // no "-0" when the sign is inverted
  this->publish_state(value == 0.0f ? 0.0f : value * this->sign_factor_());
}

}  // namespace esphome::ecoflow_stream
