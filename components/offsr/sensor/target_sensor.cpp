#include "output_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {

static const char *const TAG = "offsr.sensor";


void TargetSensor::setup() {
	
  this->parent_->add_on_pid_computed_callback([this]() { this->publish_state(this->parent_->get_target()); });	
  // this->parent_->add_on_pid_computed_callback([this]() { this->parent_->get_target(); });
  // this->parent_->add_on_pid_computed_callback([this]() { this->update_from_parent_(); });
}
/*
void TargetSensor::update_from_parent_() {
  float value;
  value = this->parent_->get_target();
  this->publish_state(value);
}
*/

// void TargetSensor::update() { this->publish_state(this->parent_->get_target()); }

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