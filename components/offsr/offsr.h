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


#ifdef USE_BINARY_SENSOR
SUB_BINARY_SENSOR(thermostat_cut)
#endif

#ifdef USE_SENSOR
SUB_SENSOR(error)
SUB_SENSOR(output)
SUB_SENSOR(target)
#endif

 public:
  
  // OFFSRComponent() = default;
  
  void setup() override;
  void dump_config() override;
  
  void set_pid_mode(bool pid_mode) { current_pid_mode_ = pid_mode; }
  void set_battery_current_sensor(sensor::Sensor *battery_current_sensor) { battery_current_sensor_ = battery_current_sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *battery_voltage_sensor) { battery_voltage_sensor_ = battery_voltage_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_device_output(output::FloatOutput *device_output) { device_output_ = device_output; }
  void pid_update();
  
  void add_on_pid_computed_callback(std::function<void()> &&callback) {
    pid_computed_callback_.add(std::move(callback));
  }
  
#ifdef USE_SWITCH
  
/*   void set_activation(bool enable) {current_activation_ = enable;}
  void set_manual_override(bool enable) {current_manual_override_ = enable;} */
 
  void set_activation(bool enable);
  void set_manual_override(bool enable);
#endif

#ifdef USE_NUMBER

/*
  void set_manual_level(float value) {current_manual_level_ = value; }
  
  void set_charging_setpoint(float value) {current_charging_setpoint_ = value;}
  void set_absorbing_setpoint(float value) {current_absorbing_setpoint_ = value;}
  void set_floating_setpoint(float value) {current_floating_setpoint_ = value;}

  void set_starting_battery_voltage(float value) {current_starting_battery_voltage_ = value;}
  void set_charged_battery_voltage(float value) {current_charged_battery_voltage_ = value;}
  void set_discharged_battery_voltage(float value) {current_discharged_battery_voltage_ = value;}
  
  void set_kp(float value) {current_kp_ = value;}
  void set_ki(float value) {current_ki_ = value;}
  void set_kd(float value) {current_kd_ = value;}
  
  void set_output_min(float value) {current_output_min_ = value;}
  void set_output_max(float value) {current_output_max_ = value;}
  void set_output_restart(float value) {current_output_restart_ = value;}
*/  
  
  void set_manual_level(float value);
  
  void set_charging_setpoint(float value);
  void set_absorbing_setpoint(float value);
  void set_floating_setpoint(float value);

  void set_starting_battery_voltage(float value);
  void set_charged_battery_voltage(float value);
  void set_discharged_battery_voltage(float value);
  
  void set_kp(float value);
  void set_ki(float value);
  void set_kd(float value);
  
  void set_output_min(float value);
  void set_output_max(float value);
  void set_output_restart(float value);
#endif

#ifdef USE_BINARY_SENSOR
bool get_thermostat_cut(void){return current_thermostat_cut_;}
#endif

#ifdef USE_SENSOR
float get_error(void){return current_error_;}
float get_output(void){return current_output_;}
float get_target(void){return current_target_;}
#endif

 protected:
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
  float current_device_output_;
  
  bool current_pid_mode_;
  
  sensor::Sensor *battery_voltage_sensor_;
  sensor::Sensor *battery_current_sensor_;
  sensor::Sensor *power_sensor_;
  output::FloatOutput *device_output_;
  
  CallbackManager<void()> pid_computed_callback_;
  
#ifdef USE_SWITCH  
  bool current_activation_;
  bool current_manual_override_;
#endif  
  
#ifdef USE_BINARY_SENSOR
  bool current_thermostat_cut_;
#endif  

#ifdef USE_SENSOR
  float current_error_ = 0.0f;
  float current_output_ = 0.0f;
  float current_target_ = 0.0f;
#endif
#ifdef USE_NUMBER
  float current_manual_level_ = 0.0f;
  
  float current_charging_setpoint_ = 20.0f;
  float current_absorbing_setpoint_ = 5.0f;
  float current_floating_setpoint_ = 0.0f;
  
  float current_starting_battery_voltage_ = 53.5;
  float current_charged_battery_voltage_ = 55.9;
  float current_discharged_battery_voltage_ = 55.8;
  
  float current_kp_;
  float current_ki_;
  float current_kd_;
  
  float current_output_max_;
  float current_output_min_;
  float current_output_restart_;
  
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

