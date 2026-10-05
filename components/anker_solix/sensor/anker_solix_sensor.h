#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../anker_solix.h"

namespace esphome::anker_solix {

/// A signed register can be offered as two entities, one per direction
enum class SplitMode : uint8_t {
  NONE,           // the value as it is
  NEGATIVE_ONLY,  // -value when the register is negative, else 0   (battery charging, grid export)
  POSITIVE_ONLY,  // value when the register is positive, else 0    (battery discharging, grid import)
};

/// Numeric register (16/32 bit, signed or not, high register first) scaled to its unit.
class AnkerSensor : public sensor::Sensor, public Component, public AnkerRegisterItem {
 public:
  void dump_config() override;
  void set_scale(float scale) { this->scale_ = scale; }
  void set_split(SplitMode split) { this->split_ = split; }
  void parse_and_publish(const std::vector<uint8_t> &data) override;

 protected:
  float scale_{1.0f};
  SplitMode split_{SplitMode::NONE};
};

/// Sum of several registers (solar power = the PV inputs of the battery + the third-party PV power). It reads them
/// itself (see AnkerDepReader): the source sensors do not have to be declared in the YAML.
class AnkerCalcSensor : public sensor::Sensor, public Component {
 public:
  static constexpr size_t MAX_DEPS = 3;

  void dump_config() override;
  void set_dep_count(uint8_t count) { this->dep_count_ = count; }
  void configure_dep(uint8_t index, uint16_t reg, uint8_t count, SensorValueType vtype, ModbusRegisterType rtype,
                     uint16_t skip_updates);
  SensorItem *get_dep(uint8_t index) { return &this->deps_[index]; }

 protected:
  void recompute_();

  uint8_t dep_count_{0};
  AnkerDepReader deps_[MAX_DEPS];
};

}  // namespace esphome::anker_solix
