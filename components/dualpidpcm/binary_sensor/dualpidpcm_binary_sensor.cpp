#include "dualpidpcm_binary_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dualpidpcm {
	
static const char *const TAG = "dualpidpcm.binarysensor";

void DUALPIDPCMBinarySensor::setup() {	
  this->parent_->add_on_pid_computed_callback([this]() { this->publish_data_(); });
}

void DUALPIDPCMBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "DUALPIDPCM Binary Sensor:");
  LOG_BINARY_SENSOR("  ", "deadband", this->deadband_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "swap", this->swap_binary_sensor_);	
}	

void DUALPIDPCMBinarySensor::publish_data_() {
  if (this->deadband_binary_sensor_ != nullptr)
    this->deadband_binary_sensor_->publish_state(this->parent_->get_deadband());

  if (this->swap_binary_sensor_ != nullptr)
    this->swap_binary_sensor_->publish_state(this->parent_->get_swap());	
}

} // dualpidpcm
} // esphome
