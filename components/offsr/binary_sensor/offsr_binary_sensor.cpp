#include "offsr_binary_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {
	
static const char *const TAG = "offsr.binarysensor";

void OFFSRBinarySensor::setup() {	
  this->parent_->add_on_pid_computed_callback([this]() { this->publish_data_(); });
}

void OFFSRBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Offsr Binary Sensor:");
  LOG_BINARY_SENSOR("  ", "Thermostat cut", this->thermostat_cut_binary_sensor_);
}	

void OFFSRBinarySensor::publish_data_() {
  if (this->thermostat_cut_binary_sensor_ != nullptr)
    this->thermostat_cut_binary_sensor_->publish_state(this->parent_->get_thermostat_cut());  
}

} // offsr
} // esphome
