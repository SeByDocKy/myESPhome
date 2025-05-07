#include "thermostat_cut_binary_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {

static const char *const TAG = "offsr.binary_sensor";

void ThermostatcutBinarySensor::setup() {
  this->parent_->add_on_pid_computed_callback([this]() { this->parent_->get_thermostat_cut(); });
}

// void ThermostatcutBinarySensor::update() { this->publish_state(this->parent_->get_thermostat_cut()); }



void ThermostatcutBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "OFFSR Thermostat cut:");
  LOG_BINARY_SENSOR("  ", "Cut", this->thermostat_cut_binary_sensor_);
 }	
	
 }
}