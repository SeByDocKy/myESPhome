#include "offsr_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {
	
static const char *const TAG = "offsr.sensor";

void OFFSRSensor::setup() {	
  this->parent_->add_on_pid_computed_callback([this]() { this->publish_data_(); });
}

void OFFSRSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Offsr Sensor:");
  LOG_SENSOR("  ", "Error", this->error_sensor_);
  LOG_SENSOR("  ", "Output", this->output_sensor_);
  LOG_SENSOR("  ", "Target", this->target_sensor_);
}	

void OFFSRSensor::publish_data_() {
  if (this->error_sensor_ != nullptr)
    this->error_sensor_->publish_state(this->parent_->get_error()); 
  if (this->output_sensor_ != nullptr)
    this->output_sensor_->publish_state(this->parent_->get_output()); 
  if (this->target_sensor_ != nullptr)
    this->target_sensor_->publish_state(this->parent_->get_target()); 
}

} // offsr
} // esphome
