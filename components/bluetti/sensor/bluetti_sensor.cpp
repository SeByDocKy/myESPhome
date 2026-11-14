#include "bluetti_sensor.h"

namespace esphome::bluetti {

static const char *const TAG = "bluetti.sensor";

void BluettiSensor::dump_config() {
  LOG_SENSOR("", "BLUETTI Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  count: %u  scale: %g", this->reg_, this->count_, this->scale_);
}

void BluettiSensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  const int64_t raw = this->raw_(data);
  switch (this->kind_) {
    case SensorKind::NIBBLE:
      this->publish_state(static_cast<float>((raw >> this->shift_) & 0xF));
      break;
    case SensorKind::REF_CURRENT:
      // A raw 0 would read as the full reference (3000 A): the pack does not report a current then
      if (raw == 0) {
        this->publish_state(NAN);
        break;
      }
      this->publish_state(static_cast<float>(std::fabs(static_cast<double>(raw) - static_cast<double>(this->reference_)) *
                                             static_cast<double>(this->scale_)));
      break;
    case SensorKind::VALUE:
    default:
      this->publish_state(static_cast<float>(static_cast<double>(raw) * static_cast<double>(this->scale_) +
                                             static_cast<double>(this->value_offset_)));
      break;
  }
}

}  // namespace esphome::bluetti
