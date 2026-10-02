#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../marstek.h"

namespace esphome::marstek {

/// Numeric register (16/32 bit, signed or not) scaled to its unit.
class MarstekSensor : public sensor::Sensor, public Component, public MarstekRegisterItem {
 public:
  void dump_config() override;
  void set_scale(float scale) { this->scale_ = scale; }
  /// EMS firmware version: a 4 digit value carries a tenth in its last digit (1476 -> 147.6), 3 digits are whole.
  void set_ems_transform(bool enable) { this->ems_transform_ = enable; }
  void parse_and_publish(const std::vector<uint8_t> &data) override;

 protected:
  float scale_{1.0f};
  bool ems_transform_{false};
};

enum class CalcMode : uint8_t {
  ROUND_TRIP,     // discharged / charged energy                 deps: charge, discharge
  CONVERSION,     // battery <-> AC conversion efficiency        deps: battery_power, ac_power
  STORED_ENERGY,  // SOC x capacity                              deps: soc, capacity
  CYCLES,         // discharged energy / capacity                deps: discharge, capacity
  SUM,            // sum of the dependencies (solar power)       deps: mppt1..mppt4
};

/// Value computed from other registers. It reads them itself (see MarstekDepReader): the source sensors do not
/// have to be declared in the YAML.
class MarstekCalcSensor : public sensor::Sensor, public Component {
 public:
  static constexpr size_t MAX_DEPS = 4;

  void dump_config() override;
  void set_mode(CalcMode mode) { this->mode_ = mode; }
  void set_dep_count(uint8_t count) { this->dep_count_ = count; }
  void configure_dep(uint8_t index, uint16_t reg, uint8_t count, SensorValueType vtype, float scale,
                     uint16_t skip_updates);
  SensorItem *get_dep(uint8_t index) { return &this->deps_[index]; }

 protected:
  void recompute_();

  CalcMode mode_{CalcMode::SUM};
  uint8_t dep_count_{0};
  MarstekDepReader deps_[MAX_DEPS];
};

}  // namespace esphome::marstek
