#include "output_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {

static const char *const TAG = "offsr.sensor";

// void OutputSensor::update() { this->publish_state(this->parent_->current_output_); }
void TargetSensor::update() { this->publish_state(this->parent_->get_target()); }
/*
void OutputSensor::write_state(float output) {
  //this->publish_state(output);
  //this->parent_->set_output(output);
  this->parent_->get_output(output);
}
*/
/*
void OutputSensor::write_state(sensor::Sensor *output_sensor) {
  // this->publish_state(output_sensor);
  this->parent_->set_output(output_sensor);
}
*/	
	
// /*	

void OutputSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "TargetSensor:");
  LOG_SENSOR("", "Target", this->target_sensor_);
 }	
// */	
	
 }
}