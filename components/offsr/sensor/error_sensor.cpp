#include "error_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {
static const char *const TAG = "offsr.sensor";

void ErrorSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "OFFSR ErrorSensor:");
  LOG_SENSOR("", "Error", this->error_sensor_);
 }	
	
	
 }
}