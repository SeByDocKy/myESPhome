#include "dualpid_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dualpid {
	
static const char *const TAG = "dualpid.sensor";

void DUALPIDSensor::setup() {	
  this->parent_->add_on_pid_computed_callback([this]() { this->publish_data_(); });
}

void DUALPIDSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "DualPID Sensor:");
  LOG_SENSOR("  ", "Error", this->error_sensor_);
  LOG_SENSOR("  ", "Output", this->output_sensor_*100.0f);
  LOG_SENSOR("  ", "Output Charging", this->output_charging_sensor_*100.0f);
  LOG_SENSOR("  ", "Output Discharging", this->output_discharging_sensor_*100.0f);
  LOG_SENSOR("  ", "Target", this->target_sensor_);
}	

void DUALPIDSensor::publish_data_() {
  if (this->error_sensor_ != nullptr)
    this->error_sensor_->publish_state(this->parent_->get_error()); 
  if (this->output_sensor_ != nullptr)
    this->output_sensor_->publish_state(this->parent_->get_output()*100.0f);
  if (this->output_charging_sensor_ != nullptr)
    this->output_charging_sensor_->publish_state(this->parent_->get_output_charging()*100.0f);
  if (this->output_discharging_sensor_ != nullptr)
    this->output_discharging_sensor_->publish_state(this->parent_->get_output_discharging()*100.0f);
  if (this->target_sensor_ != nullptr)
    this->target_sensor_->publish_state(this->parent_->get_target()); 
}

} // dualpid
} // esphome



