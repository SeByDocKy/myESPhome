#include "solarpid.h"
#include "esphome/core/log.h"

namespace esphome {
namespace solarpid {

static const char *const TAG = "solarpid";
static const float coeff = 0.001;

void SOLARPID::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up SOLARPID...");
  
  last_time_ =  millis();
  integral_  = 0.0f;

  if (this->input_sensor_ != nullptr) {
    this->input_sensor_->add_on_state_callback([this](float state) {
      this->current_input_ = state;
    });
    this->current_input_ = this->input_sensor_->state;
  }
  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->add_on_state_callback([this](float state) {
      this->current_power_ = state;
    });
    this->current_power_ = this->power_sensor_->state;
  }
  if (this->activation_switch_ != nullptr) {
    this->activation_switch_->add_on_state_callback([this](bool state) {
      this->current_activation_ = state;
    });
    this->current_activation_ = this->activation_switch_->state;
  }

}

void SOLARPID::dump_config() {
  ESP_LOGCONFIG(TAG, "SOLARPID:");
  LOG_SENSOR("", "Error", this->error_sensor_);
  LOG_SENSOR("", "PWM output", this->pwm_output_sensor_);
}

/*
void SOLARPID::write_output(float value) {
   this->output_->set_level(value);
}
*/

void SOLARPID::pid_update() {
  uint32_t now = millis();
  float tmp;
  
  dt_ = float(now - this->last_time_)/1000.0f;
  error_ = -(this->setpoint_ - this->current_input_);
  tmp = (error_ * dt_);
  if (!std::isnan(tmp)){
    integral_ += tmp;
  }
  derivative_ = (error_ - previous_error_) / dt_;
  
  if ( (!std::isnan(this->current_power_)) && (this->current_power_ < 2.0f) &&  (this->previous_pwm_output_ > this->pwm_restart_) ) {
      pwm_output_ = this->pwm_restart_;
      ESP_LOGI(TAG, "restart branch");
  }
  else{
      pwm_output_ = std::min(std::max( (coeff*this->kp_ * error_) + (coeff*this->ki_ * integral_) + (coeff*this->kd_ * derivative_) , this->output_min_  ) , this->output_max_); //
      ESP_LOGI(TAG, "full pid update branch");
  }
  //this->write_output(pwm_output);

  last_time_ = now;
  previous_error_ = error_;
  previous_pwm_output_ = pwm_output_;
  if (!this->current_activation_){
    pwm_output_ = 0.0f;
  }
  this->output_->set_level(pwm_output_);
  if (this->error_sensor_ != nullptr){
      this->error_sensor_->publish_state(error_); //error_
  }
  if (this->pwm_output_sensor_ != nullptr){
      this->pwm_output_sensor_->publish_state(pwm_output_);
  }
  ESP_LOGI(TAG, "setpoint %3.2f, Kp=%3.2f, Ki=%3.2f, Kd=%3.2f, output_min = %3.2f , output_max = %3.2f ,  previous_pwm_output_ = %3.2f , pwm_output_ = %3.2f , error_ = %3.2f, integral = %3.2f , derivative = %3.2f, current_power = %3.2f", this->setpoint_ , coeff*this->kp_ , coeff*this->ki_ , coeff*this->kd_ , this->output_min_ , this->output_max_ , previous_pwm_output_ , pwm_output_ , error_ , integral_ , derivative_ , this->current_power_);  
}


 }  // namespace solarpid
}  // namespace esphome
