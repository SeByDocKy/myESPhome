#include "solarpid.h"
#include "esphome/core/log.h"

namespace esphome {
namespace solarpid {

static const char *const TAG = "solarpid";

void SOLARPID::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up SOLARPID...");
  
  this->last_time_ =  millis();
// /*  
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
// */
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
  float pwm_output = 0.0f;
  uint32_t now = millis();
  if (this->current_activation_){
    ESP_LOGI(TAG, "activation ON");
    float dt = (now - this->last_time_)/1000.0f;
    float error = (this->setpoint_ - this->current_input_);
    this->integral_ += error * dt;
    this->derivative_ = (error - this->previous_error_) / dt;
    this->previous_error_ = error;
    // ESP_LOGI(TAG, "current_power_ != nan = %d" , !std::isnan(this->current_power_));
    //if( (!std::isnan(this->current_power_)) ) {
    if ( (!std::isnan(this->current_power_)) && (this->current_power_ < 2.0f) &&  (this->previous_pwm_output_ > this->pwm_restart_) ) {
         pwm_output = this->pwm_restart_;
         ESP_LOGI(TAG, "restart branch");
    }
    //}
    else{
      pwm_output = std::min(std::max( (this->kp_ * error) + (this->ki_ * this->integral_) + (this->kd_ * this->derivative_) , this->output_min_  ) , this->output_max_);
      ESP_LOGI(TAG, "full pid update branch");
    }
    
    if (this->error_sensor_ != nullptr){
      this->error_sensor_->publish_state(error);
    }
    if (this->pwm_output_sensor_ != nullptr){
      this->pwm_output_sensor_->publish_state(pwm_output);
    }
    ESP_LOGI(TAG, "setpoint %3.2f, Kp=%3.2f, Ki=%3.2f, Kd=%3.2f, output_min = %3.2f , output_max = %3.2f ,  previous_pwm_output = %3.2f , pwm_output = %3.2f , error = %3.2f, integral = %3.2f , derivative = %3.2f, current_power = %3.2f", this->setpoint_ , this->kp_ , this->ki_ , this->kd_ , this->output_min_ , this->output_max_ , this->previous_pwm_output_ , pwm_output , error , this->integral_ , this->derivative_ , this->current_power_);
    //this->write_output(pwm_output);
    }
  else{
    pwm_output = 0.0f;
    ESP_LOGI(TAG, "activation OFF");
    ESP_LOGI(TAG, "setpoint %3.2f, Kp=%3.2f, Ki=%3.2f, Kd=%3.2f, previous_pwm_output = %3.2f , pwm_output = %3.2f ", this->setpoint_ , this->kp_ , this->ki_ , this->kd_ , this->previous_pwm_output_ , pwm_output);
  }

  this->last_time_ = now;
  this->output_->set_level(pwm_output);
  this->previous_pwm_output_ = pwm_output;
    
}


 }  // namespace solarpid
}  // namespace esphome
