#include "output_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {


void OutputSensor::write_state(float output) {
  this->publish_state(output);
  this->parent_->set_output(output);
}

/*
void OutputSensor::write_state(sensor::Sensor *output_sensor) {
  this->publish_state(output_sensor);
  this->parent_->set_output(output_sensor);
}
*/	
	
/*	
static const char *const TAG = "offsr.sensor";
void ErrorSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "OFFSR ErrorSensor:");
  LOG_SENSOR("", "Output", this->output_sensor_);
 }	
*/	
	
 }
}