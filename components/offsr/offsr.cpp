#include "offsr.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {

static const char *const TAG = "offsr";

// /*
void OFFSRComponent::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up OFFSRComponent...");
  
  if (this->battery_current_sensor_ != nullptr) {
    this->battery_current_sensor_->add_on_state_callback([this](float state) {
      this->current_battery_current_ = state;
    });
    this->current_battery_current_ = this->battery_current_sensor_->state;
  }
  if (this->battery_voltage_sensor_ != nullptr) {
    this->battery_voltage_sensor_->add_on_state_callback([this](float state) {
      this->current_battery_voltage_ = state;
    });
    this->current_battery_voltage_ = this->battery_voltage_sensor_->state;
  }
  
  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->add_on_state_callback([this](float state) {
      this->current_power_ = state;
    });
    this->current_power_ = this->power_sensor_->state;
  }
  
}

void OFFSRComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "dump config:");
  // LOG_SENSOR("", "device output", this->device_output_);
}


void OFFSRComponent::pid_update() {
	uint32_t now = millis();	
 }

#ifdef USE_SWITCH
void OFFSRComponent::set_activation(bool enable) {
	this->current_activation_ = enable; 
}
void OFFSRComponent::set_manual_override(bool enable) {
	this->current_manual_override_ = enable;
}
#endif

// /*
#ifdef USE_BINARY_SENSOR
void OFFSRComponent::set_thermostat_cut(binary_sensor::BinarySensor bs) {
	this->current_thermostat_cut_ = bs; 
}
#endif
// */

 }  // namespace offsr
}  // namespace esphome