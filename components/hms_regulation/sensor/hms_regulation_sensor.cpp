#include "hms_regulation_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hms_regulation {
	
static const char *const TAG = "hms_regulation.sensor";

void HMS_REGULATIONSensor::setup() {	
  this->parent_->add_on_pid_computed_callback([this]() { this->publish_data_(); });
}

void HMS_REGULATIONSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "hms_regulation Sensor:");
  LOG_SENSOR("  ", "Error", this->error_sensor_);
  LOG_SENSOR("  ", "Output", this->output_sensor_);
  LOG_SENSOR("  ", "Target", this->target_sensor_);
}	

void HMS_REGULATIONSensor::publish_data_() {
  if (this->error_sensor_ != nullptr)
    this->error_sensor_->publish_state(this->parent_->get_error()); 
  if (this->output_sensor_ != nullptr)
    this->output_sensor_->publish_state(this->parent_->get_output()); 
  if (this->target_sensor_ != nullptr)
    this->target_sensor_->publish_state(this->parent_->get_target()); 
}

} // hms_regulation
} // esphome
