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
  if (this->device_output_ != nullptr) {
    this->device_output_->add_on_state_callback([this](float state) {
      this->current_output_ = state;
    });
    this->current_output_ = this->device_output_->state;
  }
  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->add_on_state_callback([this](float state) {
      this->current_power_ = state;
    });
    this->current_power_ = this->power_sensor_->state;
  }
}
// */

/*
void SOLARPID::dump_config() {
  ESP_LOGCONFIG(TAG, "dump config:");
  LOG_SENSOR("", "device output", this->device_output_);
}
*/

 }  // namespace offsr
}  // namespace esphome