#include "dualpidpcm_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dualpidpcm {
	
static const char *const TAG = "dualpidpcm.sensor";

void DUALPIDPCMSensor::setup() {	
  this->parent_->add_on_pid_computed_callback([this]() { this->publish_data_(); });
}

void DUALPIDPCMSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "DualPIDPCM Sensor:");
  LOG_SENSOR("  ", "Error", this->error_sensor_);
  LOG_SENSOR("  ", "Output", this->output_sensor_);	
  LOG_SENSOR("  ", "Output Charging", this->output_charging_sensor_);
  LOG_SENSOR("  ", "Output Discharging", this->output_discharging_sensor_);
  LOG_SENSOR("  ", "Input", this->input_sensor_);
  LOG_SENSOR("  ", "offcharge", this->offcharge_sensor_);
  LOG_SENSOR("  ", "offdischarge", this->offdischarge_sensor_);	
}	

void DUALPIDPCMSensor::publish_data_() {
  if (this->error_sensor_ != nullptr)
    this->error_sensor_->publish_state(this->parent_->get_error());
  if (this->output_sensor_ != nullptr)
    this->output_sensor_->publish_state(this->parent_->get_output()*100.0f);	
  if (this->output_charging_sensor_ != nullptr)
    this->output_charging_sensor_->publish_state(this->parent_->get_output_charging()*100.0f);
  if (this->output_discharging_sensor_ != nullptr)
    this->output_discharging_sensor_->publish_state(this->parent_->get_output_discharging()*100.0f); 
  if (this->input_sensor_ != nullptr)
    this->input_sensor_->publish_state(this->parent_->get_input());	
  if (this->offcharge_sensor_ != nullptr)
    this->offcharge_sensor_ ->publish_state(this->parent_->get_offcharge());
  if (this->offdischarge_sensor_ != nullptr)
    this->offdischarge_sensor_->publish_state(this->parent_->get_offdischarge());	
	
}

} // dualpidpcm
} // esphome








