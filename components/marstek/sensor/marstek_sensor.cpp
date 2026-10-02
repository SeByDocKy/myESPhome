#include "marstek_sensor.h"

namespace esphome::marstek {

static const char *const TAG = "marstek.sensor";

static float round_to(float value, int decimals) {
  const float factor = std::pow(10.0f, static_cast<float>(decimals));
  return std::round(value * factor) / factor;
}

void MarstekSensor::dump_config() {
  LOG_SENSOR("", "Marstek Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  count: %u  skip_updates: %u", this->reg_, this->register_count,
                this->skip_updates);
}

void MarstekSensor::parse_and_publish(const std::vector<uint8_t> &data) {
  if (!this->has_data_(data))
    return;
  const int64_t raw = this->raw_(data);
  float value;
  if (this->ems_transform_) {
    value = raw >= 1000 ? static_cast<float>(raw) / 10.0f : static_cast<float>(raw);
  } else {
    value = static_cast<float>(static_cast<double>(raw) * static_cast<double>(this->scale_));
  }
  this->publish_state(value);
}

void MarstekCalcSensor::dump_config() {
  LOG_SENSOR("", "Marstek Calculated Sensor", this);
  for (uint8_t i = 0; i < this->dep_count_; i++) {
    ESP_LOGCONFIG(TAG, "  Source %u: register %u", i, this->deps_[i].reg());
  }
}

void MarstekCalcSensor::configure_dep(uint8_t index, uint16_t reg, uint8_t count, SensorValueType vtype,
                                      float scale, uint16_t skip_updates) {
  if (index >= MAX_DEPS)
    return;
  this->deps_[index].configure(reg, count, vtype, skip_updates);
  this->deps_[index].set_scale(scale);
  this->deps_[index].set_callback([this]() { this->recompute_(); });
}

void MarstekCalcSensor::recompute_() {
  // Wait until every source has been read at least once
  for (uint8_t i = 0; i < this->dep_count_; i++) {
    if (!this->deps_[i].valid())
      return;
  }

  float result = NAN;
  switch (this->mode_) {
    case CalcMode::ROUND_TRIP: {
      const float charge = this->deps_[0].value();
      const float discharge = this->deps_[1].value();
      if (charge != 0.0f)
        result = round_to(std::min(discharge / charge * 100.0f, 100.0f), 1);
      break;
    }
    case CalcMode::CONVERSION: {
      const float battery_power = this->deps_[0].value();
      const float ac_power = this->deps_[1].value();
      float efficiency = NAN;
      if (battery_power > 0.0f) {
        if (ac_power != 0.0f)
          efficiency = std::fabs(battery_power) / std::fabs(ac_power) * 100.0f;
      } else if (battery_power != 0.0f) {
        efficiency = std::fabs(ac_power) / std::fabs(battery_power) * 100.0f;
      }
      if (!std::isnan(efficiency))
        result = round_to(std::min(efficiency, 100.0f), 1);
      break;
    }
    case CalcMode::STORED_ENERGY:
      result = round_to(this->deps_[0].value() / 100.0f * this->deps_[1].value(), 2);
      break;
    case CalcMode::CYCLES:
      if (this->deps_[1].value() != 0.0f)
        result = round_to(this->deps_[0].value() / this->deps_[1].value(), 2);
      break;
    case CalcMode::SUM: {
      float sum = 0.0f;
      for (uint8_t i = 0; i < this->dep_count_; i++)
        sum += this->deps_[i].value();
      result = round_to(sum, 2);
      break;
    }
  }
  this->publish_state(result);
}

}  // namespace esphome::marstek
