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
}

void SOLARPID::dump_config() {
  ESP_LOGCONFIG(TAG, "SOLARPID:");
  LOG_SENSOR("", "Error", this->error_sensor_);
  LOG_SENSOR("", "PWM output", this->pwm_output_sensor_);
}

void SOLARPID::pid_update() {

}


 }  // namespace solarpid
}  // namespace esphome
