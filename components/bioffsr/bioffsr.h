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
namespace bioffsr {
	
class BIOFFSRComponent : public Component{

#ifdef USE_SWITCH
SUB_SWITCH(activation)
SUB_SWITCH(manual_override)
SUB_SWITCH(pid_mode)
SUB_SWITCH(reverse)
SUB_SWITCH(output1_enable)
SUB_SWITCH(output2_enable)
#endif

#ifdef USE_NUMBER
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

 public:
  
  void setup() override;
  void dump_config() override;
  
  void set_battery_current_sensor(sensor::Sensor *battery_current_sensor) {this->battery_current_sensor_ = battery_current_sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *battery_voltage_sensor) {this->battery_voltage_sensor_ = battery_voltage_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) {this->power_sensor_ = power_sensor; }
  void set_device_output1(output::FloatOutput *device_output) {this->device_output1_ = device_output; }
  void set_device_output2(output::FloatOutput *device_output) {this->device_output2_ = device_output; }
  
  void pid_update();
  
  void add_on_pid_computed_callback(std::function<void()> &&callback) {
    pid_computed_callback_.add(std::move(callback));
  }
  
#ifdef USE_SWITCH 
  void set_activation(bool enable) {this->current_activation_ = enable;}
  bool get_activation(void){return this->current_activation_;}
  void set_manual_override(bool enable) {this->current_manual_override_ = enable;}
  bool get_manual_override(void){return this->current_manual_override_;}
  void set_pid_mode(bool enable) {this->current_pid_mode_ = enable;}
  bool get_pid_mode(void){return this->current_pid_mode_;}
  void set_reverse(bool enable) {this->current_reverse_ = enable;}
  bool get_reverse(void){return this->current_reverse_;}
  void set_output1_enable(bool enable) {this->current_output1_enable_ = enable;}
  bool get_output1_enable(void){return this->current_output1_enable_;}
  void set_output2_enable(bool enable) {this->current_output2_enable_ = enable;}
  bool get_output2_enable(void){return this->current_output2_enable_;}
  
#endif

#ifdef USE_NUMBER
  void set_charging_setpoint(float value) {this->current_charging_setpoint_ = value;}
  float get_charging_setpoint(void){return this->current_charging_setpoint_;}
  void set_absorbing_setpoint(float value) {this->current_absorbing_setpoint_ = value;}
  float get_absorbing_setpoint(void){return this->current_absorbing_setpoint_;}
  void set_floating_setpoint(float value) {this->current_floating_setpoint_ = value;}
  float get_floating_setpoint(void){return this->current_floating_setpoint_;}

  void set_starting_battery_voltage(float value) {this->current_starting_battery_voltage_ = value;}
  float get_starting_battery_voltage(void){return this->current_starting_battery_voltage_;}
  void set_charged_battery_voltage(float value) {this->current_charged_battery_voltage_ = value;}
  float get_charged_battery_voltage(void){return this->current_charged_battery_voltage_;}
  void set_discharged_battery_voltage(float value) {this->current_discharged_battery_voltage_ = value;}
  float get_discharged_battery_voltage(void){return this->current_discharged_battery_voltage_;}
  
  void set_kp(float value) {this->current_kp_ = value;}
  float get_kp(void){return this->current_kp_;}
  void set_ki(float value) {this->current_ki_ = value;}
  float get_ki(void){return this->current_ki_;}
  void set_kd(float value) {this->current_kd_ = value;}
  float get_kd(void){return this->current_kd_;}
  
  void set_output_min(float value) {this->current_output_min_ = value;}
  float get_output_min(void){return this->current_output_min_;}
  void set_output_max(float value) {this->current_output_max_ = value;}
  float get_output_max(void){return this->current_output_max_;}
  void set_output_restart(float value) {this->current_output_restart_ = value;}
  float get_output_restart(void){return this->current_output_restart_;}
#endif

#ifdef USE_BINARY_SENSOR
  bool get_thermostat_cut(void){return this->current_thermostat_cut_;}
#endif

#ifdef USE_SENSOR
  float get_error(void) { return this->current_error_; }
  float get_output(void) { return this->current_output_; }
  float get_target(void) { return this->current_target_; }
#endif

 protected:
  uint32_t last_time_ = 0;
  float dt_;
  float error_ = 0.0f;
  float previous_error_ = 0.0f;
  float output_;
  float previous_output_ = 0.0f;
  float integral_= 0.0f; 
  float derivative_ = 0.0f;
  
  float current_battery_current_ = 0.0f;
  float current_power_ = 0.0f;
  float current_battery_voltage_ = 54.0f;
  float current_device_output_ = 0.0f;
  
  sensor::Sensor *battery_voltage_sensor_;
  sensor::Sensor *battery_current_sensor_;
  sensor::Sensor *power_sensor_;
  output::FloatOutput *device_output1_;
  output::FloatOutput *device_output2_;
  
  CallbackManager<void()> pid_computed_callback_;

#ifdef USE_SENSOR
  float current_error_ = 0.0f;
  float current_output_ = 0.0f;
  float current_target_ = 0.0f;
#endif   

#ifdef USE_BINARY_SENSOR
  bool current_thermostat_cut_ = false;
#endif
  
#ifdef USE_SWITCH  
  bool current_activation_ = false;
  bool current_manual_override_ = false;
  bool current_pid_mode_ = false;
  bool current_reverse_ = false;
  bool current_output1_enable_ = true;
  bool current_output2_enable_ = false;
#endif  
  
#ifdef USE_NUMBER  
  float current_charging_setpoint_ = 20.0f;
  float current_absorbing_setpoint_ = 5.0f;
  float current_floating_setpoint_ = 0.0f;
  
  float current_starting_battery_voltage_ = 53.5f;
  float current_charged_battery_voltage_ = 55.8f;
  float current_discharged_battery_voltage_ = 55.6f;
  
  float current_kp_ = 4.0f;
  float current_ki_ = 0.0f;
  float current_kd_ = 0.0f;
  
  float current_output_max_ = 0.85f;
  float current_output_min_ = 0.18f;
  float current_output_restart_ = 0.4f;
#endif

};
		

 }  // namespace bioffsr
}  // namespace esphome


