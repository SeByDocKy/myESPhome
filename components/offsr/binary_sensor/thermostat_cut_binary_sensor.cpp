#include "thermostat_cut_binary_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {

/*	
void ThermostatcutBinarySensor::write_state(binary_sensor::BinarySensor *thermostat_cut_binary_sensor) {
  this->publish_state(thermostat_cut_binary_sensor);
  this->parent_->set_thermostat_cut(thermostat_cut_binary_sensor);
}
*/

//void ThermostatcutBinarySensor::update() { this->publish_state(this->parent_->current_thermostat_cut_); }
void ThermostatcutBinarySensor::update() { this->publish_state(this->parent_->get_thermostat_cut()); }

get_thermostat_cut

/*
void ThermostatcutBinarySensor::write_state(bool state) {
  // this->publish_state(state);
  // this->parent_->set_thermostat_cut(state);
  this->parent_->get_thermostat_cut(state);
}
*/	
	
// /*	
static const char *const TAG = "offsr.binary_sensor";
void ThermostatcutBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "OFFSR Thermostat cut:");
  LOG_BINARY_SENSOR("  ", "Cut", this->thermostat_cut_binary_sensor_);
 }	
// */ 
	
	
 }
}