#include "error_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {
	
static const char *const TAG = "offsr.sensor";

void ErrorSensor::setup() {	
  this->parent_->add_on_error_callback([this]() { this->publish_state(this->parent_->get_error()); });
  // this->parent_->add_on_pid_computed_callback([this]() { this->publish_state(this->parent_->get_error()); });
}

void ErrorSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "ErrorSensor:");
  LOG_SENSOR("", "Error", this);
/*   LOG_SENSOR("", "Error", this->error_sensor_); */
 }	
	
 }
}