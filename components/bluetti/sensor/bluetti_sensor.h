#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../bluetti.h"

namespace esphome::bluetti {

/// How the raw register value becomes the value of the sensor
enum class SensorKind : uint8_t {
  VALUE,        // raw * scale + offset
  NIBBLE,       // one 4-bit field of a packed register (the count of PV inputs)
  REF_CURRENT,  // |raw - reference| * scale: a current reported as a magnitude around a fixed reference point
};

/// Numeric register (16/32 bit, signed or not; 32 bit values come low word first) scaled to its unit.
class BluettiSensor : public sensor::Sensor, public Component, public BluettiRegisterItem {
 public:
  void dump_config() override;
  void set_kind(SensorKind kind) { this->kind_ = kind; }
  void set_scale(float scale) { this->scale_ = scale; }
  void set_offset(float offset) { this->value_offset_ = offset; }
  void set_shift(uint8_t shift) { this->shift_ = shift; }
  void set_reference(uint32_t reference) { this->reference_ = reference; }
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  SensorKind kind_{SensorKind::VALUE};
  float scale_{1.0f};
  float value_offset_{0.0f};
  uint8_t shift_{0};
  uint32_t reference_{0};
};

}  // namespace esphome::bluetti
