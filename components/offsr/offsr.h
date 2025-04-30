#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif


#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/time/real_time_clock.h"


namespace esphome {
namespace offsr {
	
class OFFSRComponent : public Component{

#ifdef USE_SWITCH
SUB_SWITCH(activation)
SUB_SWITCH(manual_override)
#endif

#ifdef USE_BINARY_SENSOR
SUB_BINARY_SENSOR(thermostat_cut)
#endif

#ifdef USE_SENSOR
SUB_SENSOR(error)
SUB_SENSOR(output)
#endif

 public:
  void setup() override;
  void dump_config() override;
 // void loop() override;
  
  void set_pid_mode(bool pid_mode) { pid_mode_ = pid_mode; }
  
  void set_battery_current_sensor(sensor::Sensor *battery_current_sensor) { battery_current_sensor_ = battery_current_sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *battery_voltage_sensor) { battery_voltage_sensor_ = battery_voltage_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  
  void set_device_output(output::FloatOutput *device_output) { device_output_ = device_output; }
  void pid_update();
  
  
#ifdef USE_SWITCH
  void set_activation(bool enable);
  void set_manual_override(bool enable);
#endif

#ifdef USE_BINARY_SENSOR
void set_thermostat_cut(binary_sensor::BinarySensor *thermostat_cut_binary_sensor);
#endif

#ifdef USE_SENSOR
void set_error(sensor::Sensor *error_sensor);
void set_output(sensor::Sensor *output_sensor);
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
  
  float current_battery_current_;
  float current_power_;
  float current_battery_voltage_;
  float current_output_;
  
  bool current_activation_;
  bool current_manual_override_;
  bool current_thermostat_cut_;
  bool pid_mode_;
  
  sensor::Sensor *battery_voltage_sensor_;
  sensor::Sensor *battery_current_sensor_;
  sensor::Sensor *power_sensor_;
  output::FloatOutput *device_output_;
  
 /*  
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *thermostat_cut_binary_sensor_{nullptr};
#endif  
 */
   
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

