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
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"


namespace esphome {
namespace offsr {
	
class OFFSRComponent : public Component{

#ifdef USE_SWITCH
SUB_SWITCH(activation)
SUB_SWITCH(manual_override)
#endif

#ifdef USE_NUMBER
SUB_NUMBER(manual_level)

SUB_NUMBER(charging_setpoint)
SUB_NUMBER(absorbing_setpoint)
SUB_NUMBER(floating_setpoint)

SUB_NUMBER(starting_battery_voltage)
SUB_NUMBER(charged_battery_voltage)
SUB_NUMBER(discharged_battery_voltage)

SUB_NUMBER(kp)
SUB_NUMBER(ki)
SUB_NUMBER(kd)

SUB_NUMBER(output_min)
SUB_NUMBER(output_max)
SUB_NUMBER(output_restart)
#endif


/* #ifdef USE_BINARY_SENSOR
SUB_BINARY_SENSOR(thermostat_cut)
#endif */

 public:
  
  void setup() override;
  void dump_config() override;
  
  void set_pid_mode(bool pid_mode) {this->current_pid_mode_ = pid_mode; }
  void set_battery_current_sensor(sensor::Sensor *battery_current_sensor) {this->battery_current_sensor_ = battery_current_sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *battery_voltage_sensor) {this->battery_voltage_sensor_ = battery_voltage_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) {this->power_sensor_ = power_sensor; }
  void set_device_output(output::FloatOutput *device_output) {this->device_output_ = device_output; }
  void pid_update();
  
  void add_on_pid_computed_callback(std::function<void()> &&callback) {
    pid_computed_callback_.add(std::move(callback));
  }
  
#ifdef USE_SWITCH 
  void set_activation(bool enable) {this->current_activation_ = enable;}
  void set_manual_override(bool enable) {this->current_manual_override_ = enable;}
#endif

#ifdef USE_NUMBER

  void set_manual_level(float value) {this->current_manual_level_ = value; }
  
  void set_charging_setpoint(float value) {this->current_charging_setpoint_ = value;}
  void set_absorbing_setpoint(float value) {this->current_absorbing_setpoint_ = value;}
  void set_floating_setpoint(float value) {this->current_floating_setpoint_ = value;}

  void set_starting_battery_voltage(float value) {this->current_starting_battery_voltage_ = value;}
  void set_charged_battery_voltage(float value) {this->current_charged_battery_voltage_ = value;}
  void set_discharged_battery_voltage(float value) {this->current_discharged_battery_voltage_ = value;}
  
  void set_kp(float value) {this->current_kp_ = value;}
  void set_ki(float value) {this->current_ki_ = value;}
  void set_kd(float value) {this->current_kd_ = value;}
  
  void set_output_min(float value) {this->current_output_min_ = value;}
  void set_output_max(float value) {this->current_output_max_ = value;}
  void set_output_restart(float value) {this->current_output_restart_ = value;}
  
#endif

// #ifdef USE_BINARY_SENSOR
  bool get_thermostat_cut(void){return this->current_thermostat_cut_;}
// #endif

  float get_error(void) { return this->current_error_; }
  float get_output(void) { return this->current_output_; }
  float get_target(void) { return this->current_target_; }

 protected:
  uint32_t last_time_ = 0;
  float dt_;
  float error_ = 0.0f;
  float previous_error_ = 0.0f;
  float output_;
  float previous_output_ = 0.0f;
  float integral_= 0.0f; 
  float derivative_ = 0.0f;
  
  float current_battery_current_;
  float current_power_;
  float current_battery_voltage_;
  float current_device_output_;
  
  bool current_pid_mode_;
  
  sensor::Sensor *battery_voltage_sensor_;
  sensor::Sensor *battery_current_sensor_;
  sensor::Sensor *power_sensor_;
  output::FloatOutput *device_output_;
  
  CallbackManager<void()> pid_computed_callback_;
  
#ifdef USE_SWITCH  
  bool current_activation_ = false;
  bool current_manual_override_ = false;
#endif  
  
#ifdef USE_BINARY_SENSOR
  bool current_thermostat_cut_ = false;
#endif  

  float current_error_ = 0.0f;
  float current_output_ = 0.0f;
  float current_target_ = 0.0f;

#ifdef USE_NUMBER
  float current_manual_level_ = 0.0f;
  
  float current_charging_setpoint_ = 20.0f;
  float current_absorbing_setpoint_ = 5.0f;
  float current_floating_setpoint_ = 0.0f;
  
  float current_starting_battery_voltage_ = 53.5f;
  float current_charged_battery_voltage_ = 55.9f;
  float current_discharged_battery_voltage_ = 55.8f;
  
  float current_kp_ = 4.0f;
  float current_ki_ = 0.1f;
  float current_kd_ = 0.5f;
  
  float current_output_max_ = 0.85f;
  float current_output_min_ = 0.19f;
  float current_output_restart_ = 0.4f;
  
#endif

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

