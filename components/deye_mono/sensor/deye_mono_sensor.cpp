#include "deye_mono_sensor.h"

namespace esphome::deye_mono {

static const char *const TAG = "deye_mono.sensor";

void DeyeMonoSensor::dump_config() {
  LOG_SENSOR("", "Deye Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  add: %g  scale: %g", this->reg_, static_cast<double>(this->transform_.add),
                static_cast<double>(this->transform_.scale));
}

void DeyeMonoSensor::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  const float value = static_cast<float>(this->transform_.apply(this->raw_(data)));
  // Nothing new to tell (force_update: true publishes anyway)
  if (this->has_state() && value == this->state && !this->get_force_update())
    return;
  this->publish_state(value);
}

void DeyeMonoCalcSensor::dump_config() {
  LOG_SENSOR("", "Deye Calculated Sensor", this);
  for (uint8_t i = 0; i < this->input_count_; i++) {
    ESP_LOGCONFIG(TAG, "  Input %u: register %u", i, this->inputs_[i].reader.reg());
  }
}

void DeyeMonoCalcSensor::add_input(uint16_t reg, SensorValueType vtype, float add, float scale, bool wrap,
                                   bool bridge, float coef) {
  if (this->input_count_ >= MAX_INPUTS)
    return;
  Input &input = this->inputs_[this->input_count_++];
  input.coef = coef;
  input.reader.configure(reg, vtype);
  input.reader.set_bridge(bridge);
  input.reader.set_transform(Transform{add, scale, wrap});
  // Computed in loop(), once the whole poll has been parsed
  input.reader.set_callback([this]() { this->enable_loop(); });
}

double DeyeMonoCalcSensor::compute_() const {
  const double out_scale = static_cast<double>(this->out_scale_);
  switch (this->op_) {
    case CalcOp::LINEAR: {
      double sum = 0.0;
      for (uint8_t i = 0; i < this->input_count_; i++)
        sum += static_cast<double>(this->inputs_[i].coef) * this->inputs_[i].reader.value();
      return sum;
    }
    case CalcOp::CHARGE_CURRENT: {
      const double current = this->inputs_[0].reader.value();
      return current >= 0.0 ? current : 0.0;
    }
    case CalcOp::DISCHARGE_CURRENT: {
      const double current = this->inputs_[0].reader.value();
      return current < 0.0 ? -current : 0.0;
    }
    case CalcOp::CHARGE_POWER: {
      const double current = this->inputs_[1].reader.value();
      return this->inputs_[0].reader.value() * (current >= 0.0 ? current : 0.0) * out_scale;
    }
    case CalcOp::DISCHARGE_POWER: {
      const double current = this->inputs_[1].reader.value();
      return this->inputs_[0].reader.value() * (current < 0.0 ? -current : 0.0) * out_scale;
    }
  }
  return 0.0;
}

void DeyeMonoCalcSensor::loop() {
  this->disable_loop();
  // Wait until every input has been read at least once
  for (uint8_t i = 0; i < this->input_count_; i++) {
    if (!this->inputs_[i].reader.valid())
      return;
  }
  float value = static_cast<float>(this->compute_());
  if (value == 0.0f)
    value = 0.0f;  // not -0 (the discharge power is a negated product)
  if (this->has_state() && value == this->state && !this->get_force_update())
    return;
  this->publish_state(value);
}

}  // namespace esphome::deye_mono
