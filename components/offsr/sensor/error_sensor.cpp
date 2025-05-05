#include "error_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {
/*	
void ErrorSensor::write_state(float error) {
  this->publish_state(error);
  this->parent_->set_error(error);
}
*/	
// /*	
void ErrorSensor::write_state(sensor::Sensor *error_sensor) {
  this->publish_state(error_sensor);
  this->parent_->set_error(error_sensor);
}	
// */

/*	
static const char *const TAG = "offsr.sensor";
void ErrorSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "OFFSR ErrorSensor:");
  LOG_SENSOR("", "Error", this->error_sensor_);
 }	
*/
	
 }
}