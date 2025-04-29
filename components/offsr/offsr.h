#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/time/real_time_clock.h"


namespace esphome {
namespace offsr {
	
class OFFSRComponent : public Component{
#ifdef USE_SWITCH
SUB_SWITCH(activation)
#endif
 public:
  // OFFSRComponent();
  void setup() override;
  void dump_config() override;
  void loop() override;
  
  void set_pid_option(bool pid) { pid_ = pid; }
  
  void set_input_sensor(sensor::Sensor *input_sensor) { input_sensor_ = input_sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *battery_voltage_sensor) { battery_voltage_sensor_ = battery_voltage_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  
  void set_device_output(output::FloatOutput *device_output) { device_output_ = device_output; }
  void pid_update();
  
  
#ifdef USE_SWITCH
  void set_activation(bool enable);
#endif
 protected:
  float setpoint_ , kp_ , ki_ , kd_ , output_min_ , output_max_ , output_restart_ , starting_battery_voltage_; 
  uint32_t last_time_ = 0;
  float dt_;
  float error_;
  float previous_error_ = 0.0f;
  float output_;
  float previous_output_ = 0.0f;
  float integral_= 0.0f; 
  float derivative_ = 0.0f;
  float current_input_;
  float current_power_;
  float current_battery_voltage_;
  bool current_activation_;
  bool current_thermostat_cut_;
  bool pid_;
  
  sensor::Sensor *input_sensor_;
  sensor::Sensor *power_sensor_;
  sensor::Sensor *battery_voltage_sensor_;
  output::FloatOutput *device_output_;
   
};
	
template<typename... Ts> 
class PidUpdateAction : public Action<Ts...> {
 public:
  PidUpdateAction(OFFSRComponent *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->pid_update(); }  

 protected:
  OFFSRComponent *parent_;
};	
	
	
 }  // namespace offsr
}  // namespace esphome

