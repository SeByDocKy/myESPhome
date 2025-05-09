#include "output_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {

static const char *const TAG = "offsr.sensor";

void OutputSensor::setup() {
  // this->parent_->add_on_pid_computed_callback([this]() { this->publish_state(this->parent_->get_output()); });
  this->parent_->add_on_output_callback([this]() { this->publish_state(this->parent_->get_output()); });
}

void OutputSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "OutputSensor:");
/*   LOG_SENSOR("", "Output", this->output_sensor_); */
  LOG_SENSOR("", "Error", this);
 }	
	
 }
}