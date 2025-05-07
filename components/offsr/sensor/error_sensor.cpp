#include "error_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {
	
static const char *const TAG = "offsr.sensor";

void ErrorSensor::setup() {
  // this->parent_->add_on_pid_computed_callback([this]() { this->parent_->get_error(); });
  this->parent_->add_on_pid_computed_callback([this]() { this->update_from_parent_(); });
}
void ErrorSensor::update_from_parent_() {
  float value;
  value = this->parent_->get_error();
  this->publish_state(value);
}


// void ErrorSensor::update() { this->publish_state(this->parent_->get_error()); }


/*
void ErrorSensor::write_state(float error) {
  // this->publish_state(error);
  // this->parent_->set_error(error);
  this->parent_->get_error(error);
}
*/	
/*	
void ErrorSensor::write_state(sensor::Sensor *error_sensor) {
  // this->publish_state(error_sensor);
  this->parent_->set_error(error_sensor);
}	
*/

// /*	

void ErrorSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "ErrorSensor:");
  LOG_SENSOR("", "Error", this->error_sensor_);
 }	
// */
	
 }
}