#include "dualpid_binary_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dualpid {
	
static const char *const TAG = "dualpid.binarysensor";

void DUALPIDBinarySensor::setup() {	
  this->parent_->add_on_pid_computed_callback([this]() { this->publish_data_(); });
}

void DUALPIDBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "DUALPID Binary Sensor:");
  LOG_BINARY_SENSOR("  ", "deadband", this->deadband_binary_sensor_);
}	

void DUALPIDPCMBinarySensor::publish_data_() {
  if (this->deadband_binary_sensor_ != nullptr)
    this->deadband_binary_sensor_->publish_state(this->parent_->get_deadband());  
}

} // dualpid
} // esphome
