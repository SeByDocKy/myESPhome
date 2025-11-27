#include "bioffsr_binary_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bioffsr {
	
static const char *const TAG = "bioffsr.binarysensor";

void BIOFFSRBinarySensor::setup() {	
  this->parent_->add_on_pid_computed_callback([this]() { this->publish_data_(); });
}

void BIOFFSRBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Bioffsr binary Sensor:");
  LOG_BINARY_SENSOR("  ", "Thermostat cut", this->thermostat_cut_binary_sensor_);
}	

void BIOFFSRBinarySensor::publish_data_() {
  if (this->thermostat_cut_binary_sensor_ != nullptr)
    this->thermostat_cut_binary_sensor_->publish_state(this->parent_->get_thermostat_cut());  
}

} // bioffsr
} // esphome
