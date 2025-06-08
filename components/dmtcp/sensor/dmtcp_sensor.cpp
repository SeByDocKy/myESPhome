#include "dmtcp_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dmtcp {
	
static const char *const TAG = "offsr.sensor";

void DMTCPSensor::setup() {	
  this->parent_->add_on_dmtcp_callback([this]() { this->publish_data_(); });
}

void DMTCPSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Offsr Sensor:");
  LOG_SENSOR("  ", "PV1 Voltage", this->pv1_voltage_);
}	

void DMTCPSensor::publish_data_() {
  if (this->pv1_voltage_ != nullptr)
    this->pv1_voltage_->publish_state(this->parent_->get_pv1_voltage());  
}

} // dmtcp
} // esphome