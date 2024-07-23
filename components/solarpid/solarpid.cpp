#include "solarpid.h"
#include "esphome/core/log.h"

namespace esphome {
namespace solarpid {

void SOLARPID::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up SOLARPID..."); 
}

void SOLARPID::dump_config() {
  ESP_LOGCONFIG(TAG, "SOLARPID:");
  LOG_SENSOR("", "Error", this->error_sensor_);
  LOG_SENSOR("", "PWM output", this->pwm_output_sensor_);
}



 }  // namespace solarpid
}  // namespace esphome
