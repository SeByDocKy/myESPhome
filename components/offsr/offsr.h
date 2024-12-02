#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"


namespace esphome {
namespace offsr {
	
class OFFSRComponent : public Component{
#ifdef USE_SWITCH
SUB_SWITCH(activation)
#endif
 public:
  OFFSRComponent();
  void setup() override;
  void dump_config() override;
  void loop() override;
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
   
}
	
	
	
 }  // namespace offsr
}  // namespace esphome

