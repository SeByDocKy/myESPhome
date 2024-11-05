#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace solarpid {

class SOLARPID : public Component {
 public:
  void set_setpoint(float setpoint) { setpoint_ = setpoint; }
  void set_kp(float kp) { kp_ = kp; }
  void set_ki(float ki) { ki_ = ki; }
  void set_kd(float kd) { kd_ = kd; }
  void set_output_min(float output_min) { output_min_ = output_min; }
  void set_output_max(float output_max) { output_max_ = output_max; }
  void set_pwm_restart(float pwm_restart) { pwm_restart_ = pwm_restart; }
  void set_starting_battery_voltage (float starting_battery_voltage) { starting_battery_voltage_ = starting_battery_voltage    ;}
  void set_activation_switch(switch_::Switch *activation_switch) {activation_switch_ = activation_switch;}
  void set_input_sensor(sensor::Sensor *input_sensor) { input_sensor_ = input_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_output(output::FloatOutput *output) { output_ = output; }
  void set_error(sensor::Sensor *error_sensor) { error_sensor_ = error_sensor; }
  void set_pwm_output(sensor::Sensor *pwm_output_sensor) { pwm_output_sensor_ = pwm_output_sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *battery_voltage_sensor) { battery_voltage_sensor_ = battery_voltage_sensor; }
  
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  // void loop() override;
  void pid_update();
 protected:
  //void pid_update();
 

  //void write_output(float)
  
  float setpoint_ , kp_ , ki_ , kd_ , output_min_ , output_max_ , pwm_restart_ , starting_battery_voltage_; 
  uint32_t last_time_ = 0;
  float dt_;
  float error_;
  float previous_error_ = 0.0f;
  float pwm_output_;
  float previous_pwm_output_ = 0.0f;
  float integral_= 0.0f; 
  float derivative_ = 0.0f;
  float current_point;
  float current_input_;
  float current_power_;
  float current_battery_voltage_;
  bool current_activation_;
 
  switch_::Switch *activation_switch_;
  sensor::Sensor *input_sensor_;
  sensor::Sensor *power_sensor_;
  sensir::Sensor *battery_voltage_sensor_;
  output::FloatOutput *output_;

  sensor::Sensor *error_sensor_{nullptr};
  sensor::Sensor *pwm_output_sensor_{nullptr};
};

template<typename... Ts> 
class SetPointAction : public Action<Ts...> {
 public:
  SetPointAction(SOLARPID *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, new_setpoint)
  void play(Ts... x) override { this->parent_->set_setpoint(this->new_setpoint_.value(x...) ); }
  
 protected:
  SOLARPID *parent_;
};

template<typename... Ts> 
class SetKpAction : public Action<Ts...> {
 public:
  SetKpAction(SOLARPID *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, new_kp)
  void play(Ts... x) override { this->parent_->set_kp(this->new_kp_.value(x...) ); }
  
 protected:
   SOLARPID *parent_;
};

template<typename... Ts> 
class SetKiAction : public Action<Ts...> {
 public:
  SetKiAction(SOLARPID *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, new_ki)
  void play(Ts... x) override { this->parent_->set_ki(this->new_ki_.value(x...) ); }

 protected:
    SOLARPID *parent_;
};

template<typename... Ts> 
class SetKdAction : public Action<Ts...> {
 public:
  SetKdAction(SOLARPID *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, new_kd)
  void play(Ts... x) override { this->parent_->set_kd(this->new_kd_.value(x...) ); }
  
 protected:
    SOLARPID *parent_;
};


template<typename... Ts> 
class SetOutputMinAction : public Action<Ts...> {
 public:
  SetOutputMinAction(SOLARPID *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, new_output_min)
  void play(Ts... x) override { this->parent_->set_output_min(this->new_output_min_.value(x...) ); }
  
 protected:
   SOLARPID *parent_;
};

template<typename... Ts> 
class SetOutputMaxAction : public Action<Ts...> {
 public:
  SetOutputMaxAction(SOLARPID *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, new_output_max)
  void play(Ts... x) override { this->parent_->set_output_max(this->new_output_max_.value(x...) ); }
  
 protected:
   SOLARPID *parent_;
};

template<typename... Ts> 
class SetPwmRestartAction : public Action<Ts...> {
 public:
  SetPwmRestartAction(SOLARPID *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, new_pwm_restart)
  void play(Ts... x) override { this->parent_->set_pwm_restart(this->new_pwm_restart_.value(x...) ); }
  
 protected:
   SOLARPID *parent_;
};

template<typename... Ts> 
class SetStartingBatteryVoltageAction : public Action<Ts...> {
 public:
  SetStartingBatteryVoltageAction(SOLARPID *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, new_starting_battery_voltage)
  void play(Ts... x) override { this->parent_->set_starting_battery_voltage(this->new_starting_battery_voltage_.value(x...) ); }
  
 protected:
   SOLARPID *parent_;
};


template<typename... Ts> 
class PidUpdateAction : public Action<Ts...> {
 public:
  PidUpdateAction(SOLARPID *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->pid_update(); }  

 protected:
  SOLARPID *parent_;
};

}  // namespace solarpid
}  // namespace esphome
