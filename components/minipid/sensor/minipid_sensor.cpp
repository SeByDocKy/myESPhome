#include "minipid_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace minipid {
	
static const char *const TAG = "minipid.sensor";

void MINIPIDSensor::setup() {	
  this->parent_->add_on_pid_computed_callback([this]() { this->publish_data_(); });
}

void MINIPIDSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "minipid Sensor:");
  LOG_SENSOR("  ", "Error", this->error_sensor_);
  LOG_SENSOR("  ", "Output", this->output_sensor_);
  LOG_SENSOR("  ", "Target", this->target_sensor_);
}	

void MINIPIDSensor::publish_data_() {
  if (this->error_sensor_ != nullptr)
    this->error_sensor_->publish_state(this->parent_->get_error()); 
  if (this->output_sensor_ != nullptr)
    this->output_sensor_->publish_state(this->parent_->get_output()*100); 
  if (this->target_sensor_ != nullptr)
    this->target_sensor_->publish_state(this->parent_->get_target()); 
}

} // minipid
} // esphome

