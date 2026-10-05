#include "anker_solix_sensor.h"

namespace esphome::anker_solix {

static const char *const TAG = "anker_solix.sensor";

void AnkerSensor::dump_config() {
  LOG_SENSOR("", "Anker SOLIX Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  count: %u  scale: %g  skip_updates: %u", this->reg_, this->register_count,
                this->scale_, this->skip_updates);
}

void AnkerSensor::parse_and_publish(const std::vector<uint8_t> &data) {
  if (!this->has_data_(data))
    return;
  double value = static_cast<double>(this->raw_(data));
  switch (this->split_) {
    case SplitMode::NEGATIVE_ONLY:
      value = value < 0.0 ? -value : 0.0;
      break;
    case SplitMode::POSITIVE_ONLY:
      value = value > 0.0 ? value : 0.0;
      break;
    case SplitMode::NONE:
      break;
  }
  this->publish_state(static_cast<float>(value * static_cast<double>(this->scale_)));
}

void AnkerCalcSensor::dump_config() {
  LOG_SENSOR("", "Anker SOLIX Calculated Sensor", this);
  for (uint8_t i = 0; i < this->dep_count_; i++) {
    ESP_LOGCONFIG(TAG, "  Source %u: register %u", i, this->deps_[i].reg());
  }
}

void AnkerCalcSensor::configure_dep(uint8_t index, uint16_t reg, uint8_t count, SensorValueType vtype,
                                    ModbusRegisterType rtype, uint16_t skip_updates) {
  if (index >= MAX_DEPS)
    return;
  this->deps_[index].configure(reg, count, vtype, rtype, skip_updates);
  this->deps_[index].set_callback([this](int64_t) { this->recompute_(); });
}

void AnkerCalcSensor::recompute_() {
  // Wait until every source has been read at least once
  double sum = 0.0;
  for (uint8_t i = 0; i < this->dep_count_; i++) {
    if (!this->deps_[i].valid())
      return;
    sum += static_cast<double>(this->deps_[i].value());
  }
  this->publish_state(static_cast<float>(sum));
}

}  // namespace esphome::anker_solix
