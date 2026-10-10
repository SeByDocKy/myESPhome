#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../deye_mono.h"

#include <array>

namespace esphome::deye_mono {

/// Numeric register (16/32 bit, signed or not) converted to its unit: (raw + add) * scale.
class DeyeMonoSensor : public sensor::Sensor, public Component, public DeyeMonoItem {
 public:
  void dump_config() override;
  void set_transform(float add, float scale, bool wrap) { this->transform_ = Transform{add, scale, wrap}; }
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  Transform transform_{};
};

/// What a calculated sensor does with the values of its input registers
enum class CalcOp : uint8_t {
  LINEAR,             // sum of coef[i] * input[i]
  CHARGE_CURRENT,     // input[0] when it is >= 0, else 0
  DISCHARGE_CURRENT,  // -input[0] when it is < 0, else 0
  CHARGE_POWER,       // input[0] (voltage) * charge current of input[1] * out_scale
  DISCHARGE_POWER,    // input[0] (voltage) * discharge current of input[1] * out_scale
};

/** Sensor computed from registers it reads itself (see DeyeMonoDepReader): the source sensors do not have to be
 *  declared in the YAML. It is computed once per poll, after the registers of the poll have been parsed (all the
 *  inputs of one poll are then of the same poll), and only published when its value changed. */
class DeyeMonoCalcSensor : public sensor::Sensor, public Component {
 public:
  static constexpr size_t MAX_INPUTS = 3;

  void setup() override { this->disable_loop(); }
  void loop() override;
  void dump_config() override;
  void set_op(CalcOp op, float out_scale) {
    this->op_ = op;
    this->out_scale_ = out_scale;
  }
  void add_input(uint16_t reg, SensorValueType vtype, float add, float scale, bool wrap, bool bridge, float coef);
  SensorItem *get_input(uint8_t index) { return &this->inputs_[index].reader; }

 protected:
  struct Input {
    DeyeMonoDepReader reader;
    float coef{1.0f};
  };
  double compute_() const;

  CalcOp op_{CalcOp::LINEAR};
  float out_scale_{1.0f};
  uint8_t input_count_{0};
  std::array<Input, MAX_INPUTS> inputs_;
};

}  // namespace esphome::deye_mono
