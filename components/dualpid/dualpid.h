#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h"
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/time/real_time_clock.h"


namespace esphome {
namespace dualpid {
	
class DUALPIDComponent : public Component{

#ifdef USE_SWITCH

SUB_SWITCH(activation)
SUB_SWITCH(manual_override)
SUB_SWITCH(pid_mode)
SUB_SWITCH(reverse)
// SUB_SWITCH(r48)

#endif

#ifdef USE_NUMBER

SUB_NUMBER(setpoint)

SUB_NUMBER(charging_epoint)
SUB_NUMBER(absorbing_epoint)
SUB_NUMBER(floating_epoint)

SUB_NUMBER(starting_battery_voltage)
SUB_NUMBER(charged_battery_voltage)
SUB_NUMBER(discharged_battery_voltage)

SUB_NUMBER(kp_charging)
SUB_NUMBER(ki_charging)
SUB_NUMBER(kd_charging)

SUB_NUMBER(kp_discharging)
SUB_NUMBER(ki_discharging)
SUB_NUMBER(kd_discharging)

SUB_NUMBER(output_min)
SUB_NUMBER(output_max)
SUB_NUMBER(output_min_charging)
SUB_NUMBER(output_max_charging)
SUB_NUMBER(output_min_discharging)
SUB_NUMBER(output_max_discharging)

#endif

 public:
  
  void setup() override;
  void dump_config() override;
  
  void set_input_sensor(sensor::Sensor *input_sensor) {this->input_sensor_ = input_sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *battery_voltage_sensor) {this->battery_voltage_sensor_ = battery_voltage_sensor; }
  void set_device_charging_output(output::FloatOutput *output) {this->device_charging_output_ = output; }
  void set_device_discharging_output(output::FloatOutput *output) {this->device_discharging_output_ = output; }
  void set_r48_general_switch(switch_::Switch *sw) {r48_general_switch_ = sw;}
  void set_producing_binary_sensor (binary_sensor::BinarySensor *bs) {producing_binary_sensor_ = bs;}
  
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
  // void set_r48(bool enable) {this->current_r48_ = enable;}
  // bool get_r48(void){return this->current_r48_;}
#endif

#ifdef USE_NUMBER

  void set_setpoint(float value) {this->current_setpoint_ = value;}
  float get_setpoint(void){return this->current_setpoint_;}
  
  void set_charging_epoint(float value) {this->current_charging_epoint_ = value;}
  float get_charging_epoint(void){return this->current_charging_epoint_;}
  void set_absorbing_epoint(float value) {this->current_absorbing_epoint_ = value;}
  float get_absorbing_epoint(void){return this->current_absorbing_epoint_;}
  void set_floating_epoint(float value) {this->current_floating_epoint_ = value;}
  float get_floating_epoint(void){return this->current_floating_epoint_;}
  
  void set_starting_battery_voltage(float value) {this->current_starting_battery_voltage_ = value;}
  float get_starting_battery_voltage(void){return this->current_starting_battery_voltage_;}
  
  void set_charged_battery_voltage(float value) {this->current_charged_battery_voltage_ = value;}
  float get_charged_battery_voltage(void){return this->current_charged_battery_voltage_;}
  void set_discharged_battery_voltage(float value) {this->current_discharged_battery_voltage_ = value;}
  float get_discharged_battery_voltage(void){return this->current_discharged_battery_voltage_;}
  
  void set_kp_charging(float value) {this->current_kp_charging_ = value;}
  float get_kp_charging(void){return this->current_kp_charging_;}
  void set_ki_charging(float value) {this->current_ki_charging_ = value;}
  float get_ki_charging(void){return this->current_ki_charging_;}
  void set_kd_charging(float value) {this->current_kd_charging_ = value;}
  float get_kd_charging(void){return this->current_kd_charging_;}
  
  void set_kp_discharging(float value) {this->current_kp_discharging_ = value;}
  float get_kp_discharging(void){return this->current_kp_discharging_;}
  void set_ki_discharging(float value) {this->current_ki_discharging_ = value;}
  float get_ki_discharging(void){return this->current_ki_discharging_;}
  void set_kd_discharging(float value) {this->current_kd_discharging_ = value;}
  float get_kd_discharging(void){return this->current_kd_discharging_;}
  
  void set_output_min(float value) {this->current_output_min_ = value;}
  float get_output_min(void){return this->current_output_min_;}
  void set_output_max(float value) {this->current_output_max_ = value;}
  float get_output_max(void){return this->current_output_max_;}

  void set_output_min_charging(float value) {this->current_output_min_charging_ = value;}
  float get_output_min_charging(void){return this->current_output_min_charging_;}
  void set_output_max_charging(float value) {this->current_output_max_charging_ = value;}
  float get_output_max_charging(void){return this->current_output_max_charging_;}

  void set_output_min_discharging(float value) {this->current_output_min_discharging_ = value;}
  float get_output_min_discharging(void){return this->current_output_min_discharging_;}
  void set_output_max_discharging(float value) {this->current_output_max_discharging_ = value;}
  float get_output_max_discharging(void){return this->current_output_max_discharging_;}
  
#endif


#ifdef USE_SENSOR
  float get_error(void) { return this->current_error_; }
  float get_output(void) { return this->current_output_; }
  float get_output_charging(void) { return this->current_output_charging_; }
  float get_output_discharging(void) { return this->current_output_discharging_; }  
  float get_target(void) { return this->current_target_; }
  float get_epoint(void) { return this->current_epoint_; }
#endif

 protected:
  uint32_t last_time_ = 0;
  float dt_;
  float error_ = 0.0f;
  float previous_error_ = 0.0f;
  float output_ = 0.5f;
  float output_charging_ = 0.0f;
  float output_discharging_ = 0.0f;
  float previous_output_ = 0.5f;
  float integral_= 0.0f; 
  float derivative_ = 0.0f;
  
  float current_charging_epoint_ = 0.5f;
  float current_absorbing_epoint_ = 0.2f;
  float current_floating_epoint_ = 0.0f;
  
  float current_input_ = 0.0f;
  float current_battery_voltage_ = 54.0f;
  float current_device_output_charging_ = 0.0f;
  float current_device_output_discharging_ = 0.0f;
  
  sensor::Sensor *input_sensor_;
  sensor::Sensor *battery_voltage_sensor_;
  output::FloatOutput *device_charging_output_; 
  output::FloatOutput *device_discharging_output_;
  switch_::Switch  *r48_general_switch_;
  binary_sensor::BinarySensor producing_binary_sensor_;
  
  CallbackManager<void()> pid_computed_callback_;

#ifdef USE_SENSOR
  float current_error_ = 0.0f;
  float current_output_ = 0.0f;
  float current_output_charging_ = 0.0f;
  float current_output_discharging_ = 0.0f;
  float current_target_ = 0.0f;
  float current_epoint_ = 0.5f;
#endif   

#ifdef USE_SWITCH  
  bool current_activation_ = false;
  bool current_manual_override_ = false;
  bool current_pid_mode_ = false;
  bool current_reverse_ = false;
  // bool current_r48_ = false;
#endif  
  
#ifdef USE_NUMBER
  float current_setpoint_ = 0.0f;

  float current_starting_battery_voltage_ = 53.5f;
  float current_charged_battery_voltage_ = 55.8f;
  float current_discharged_battery_voltage_ = 55.6f;
  
  float current_kp_ = 4.0f;
  float current_ki_ = 0.0f;
  float current_kd_ = 0.0f;
  
  float current_kp_charging_ = 4.0f;
  float current_ki_charging_ = 0.0f;
  float current_kd_charging_ = 0.0f;
  
  float current_kp_discharging_ = 4.0f;
  float current_ki_discharging_ = 0.0f;
  float current_kd_discharging_ = 0.0f;  
  
  float current_output_max_ = 1.0f;
  float current_output_min_ = 0.0f;

  float current_output_max_charging_ = 1.0f;
  float current_output_min_charging_ = 0.02f;

  float current_output_max_discharging_ = 1.0f;
  float current_output_min_discharging_ = 0.05f;

#endif

};
		
 }  // namespace dualpid
}  // namespace esphome







