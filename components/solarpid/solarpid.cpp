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

void SOLARPID::pid_update() {
  if (this->current_activation_){
    uint32_t now = millis();
    float dt = (now - this->last_time_)/1000.0f;
    float error = (this->setpoint_ - this->current_input_);
    this->integral_ += error * dt;
    this->derivative_ = (error - this->previous_error_) / dt;
    this->previous_error_ = error;
    float pwm_output = std::min(std::max( (this->kp_ * error) + (this->ki_ * this->integral_) + (this->kd_ * this->derivative_) , this->output_min_  ) , this->output_max_);
    if (this->error_sensor_ != nullptr){
      this->error_sensor_->publish_state(error);
    }
    if (this->pwm_output_sensor_ != nullptr){
      this->pwm_output_sensor_->publish_state(pwm_output);
    }
    this->last_time_ = now;
    this->output_->set_level(pwm_output);
  }
}


 }  // namespace solarpid
}  // namespace esphome
