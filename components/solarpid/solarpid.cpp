#include "solarpid.h"
#include "esphome/core/log.h"

namespace esphome {
namespace solarpid {

void SOLARPID::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up SOLARPID...");
  
  this->last_time =  millis();
  
  if (this->input_sensor_ != nullptr) {
    this->input_sensor_->add_on_state_callback([this](float state) {
      this->current_input = state;
      this->publish_state();
    });
    this->current_input = this->input_sensor_->state;
  }
  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->add_on_state_callback([this](float state) {
      this->current_power = state;
      this->publish_state();
    });
    this->current_power = this->power_sensor_->state;
  }
  if (this->activation_binary_sensor_ != nullptr) {
    this->activation_binary_sensor_->add_on_state_callback([this](bool state) {
      this->current_activation = state;
      this->publish_state();
    });
    this->current_activation = this->activation_binary_sensor_->state;
  }

}

void SOLARPID::dump_config() {
  ESP_LOGCONFIG(TAG, "SOLARPID:");
  LOG_SENSOR("", "Error", this->error_sensor_);
  LOG_SENSOR("", "PWM output", this->pwm_output_sensor_);
}

void SOLARPID::pid_update() {
  //double now = millis();
  if (this->current_activation){
    uint32_t now = millis();
    float dt = (now - this->last_time)/1000.0f;
    float error = (this->currentpoint - this->current_input);
    this->integral += error * dt;
    this->derivative = (error - this->previous_error) / dt;
    this->previous_error = error;
    float pwm_output = std::min(std::max( (this->kp_ * error) + (this->ki_ * this->integral) + (this->kd_ * this->derivative) , this->output_min_  ) , this->output_max_);
    if (this->error_ != nullptr){
      this->error_->publish_state(error);
    }
    if (this->pwm_output_sensor_ != nullptr){
      this->pwm_output_sensor_->publish_state(pwm_output);
    }
    this->last_time = now;
    this->output_->set_level(pwm_output);

  }

}


 }  // namespace solarpid
}  // namespace esphome
