#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace solarpid {

class SOLARPIDComponent : public PollingComponent {
# class SOLARPIDComponent : public sensor::Sensor, public Component {
 public:

 
  void set_setpoint(float setpoint) { setpoint_ = setpoint; }
  void set_kp(float kp) { kp_ = kp; }
  void set_ki(float ki) { ki_ = ki; }
  void set_kd(float kd) { kd_ = kd; }
  void set_output_min(float output_min) { output_min_ = output_min; }
  void set_output_max(float output_max) { output_max_ = output_max; }
  void set_pwm_restart(float pwm_restart) { pwm_restart_ = pwm_restart; }

  void set_input_sensor(sensor::Sensor *input_sensor) { input_sensor_ = input_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }

  void set_output(output::FloatOutput *output) { output_ = output; }
  

  void set_error(sensor::Sensor *error_sensor) { error_sensor_ = error_sensor; }
  void set_pwm_output(sensor::Sensor *pwm_output_sensor) { pwm_output_sensor_ = pwm_output_sensor; }


  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void loop() override;

 protected:
  float setpoint_ , kp_ , ki_ , kd_ , output_min_ , output_max_ , pwm_restart_; 
  // ESPPreferenceObject pref_;
  time::RealTimeClock *time_;
  sensor::Sensor *input_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  output::FloatOutput *output_{nullptr};

};

/*
template<typename... Ts> class STATISTICSresetaction : public Action<Ts...> {
 public:
  STATISTICSresetaction(STATISTICSComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->reset(); }

 protected:
  STATISTICSComponent *parent_;
};
*/
}  // namespace solarpid
}  // namespace esphome
