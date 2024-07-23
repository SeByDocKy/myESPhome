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
  double dt = (millis() - this->last_time)/1000.00;
  this->error_ = (this->currentpoint - actual);
  //double proportional = error;
  this->integral += this->error_ * dt;
  double derivative = (this->error_ - this->previous) / dt;
  this->previous = this->error_;
  double output = (kp * this->error_) + (ki * this->integral) + (kd * this->derivative);

}


 }  // namespace solarpid
}  // namespace esphome
