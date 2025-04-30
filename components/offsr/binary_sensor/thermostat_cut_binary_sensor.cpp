#include "thermostat_cut_binary_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {
static const char *const TAG = "offsr.binary_sensor";

void ThermostatcutBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "OFFSR Thermostat cut:");
  LOG_BINARY_SENSOR("  ", "Cut", this->thermostat_cut_binary_sensor_);
 }	
	
	
 }
}