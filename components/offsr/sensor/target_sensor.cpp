#include "target_sensor.h"
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


void TargetSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "TargetSensor:");
  LOG_SENSOR("", "Target", this->target_sensor_);
 }	
	
 }
}