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

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"


namespace esphome {
namespace minipid {
	
class MINIPIDComponent : public Component{

#ifdef USE_SWITCH
SUB_SWITCH(activation)
SUB_SWITCH(manual_override)
SUB_SWITCH(pid_mode)
SUB_SWITCH(reverse)
#endif

#ifdef USE_NUMBER
SUB_NUMBER(setpoint)

SUB_NUMBER(kp)
SUB_NUMBER(ki)
SUB_NUMBER(kd)

SUB_NUMBER(output_min)
SUB_NUMBER(output_max)

#endif

 public:
  
  void setup() override;
  void dump_config() override;
  
  void set_input_sensor(sensor::Sensor *input_sensor) {this->input_sensor_ = input_sensor; }
  void set_device_output(output::FloatOutput *device_output) {this->device_output_ = device_output; }
  
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
#endif

#ifdef USE_NUMBER
  void set_setpoint(float value) {this->current_setpoint_ = value;}
  float get_setpoint(void){return this->current_setpoint_;}
  
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
  
  float current_input_ = 0.0f;
  float current_device_output_ = 0.0f;
  
  sensor::Sensor *input_sensor_;
  output::FloatOutput *device_output_;
  
  CallbackManager<void()> pid_computed_callback_;

#ifdef USE_SENSOR
  float current_error_ = 0.0f;
  float current_output_ = 0.0f;
  float current_target_ = 0.0f;
#endif   

#ifdef USE_SWITCH  
  bool current_activation_ = false;
  bool current_manual_override_ = false;
  bool current_pid_mode_ = false;
  bool current_reverse_ = false;
#endif  
  
#ifdef USE_NUMBER  
  float current_setpoint_ = 0.0f;
  
  float current_kp_ = 4.0f;
  float current_ki_ = 0.0f;
  float current_kd_ = 0.0f;
  
  float current_output_max_ = 100.0f;
  float current_output_min_ = 2.0f;
#endif

};
		

 }  // namespace minipid
}  // namespace esphome



