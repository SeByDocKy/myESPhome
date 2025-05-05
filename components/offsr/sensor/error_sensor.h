#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

/*
class ErrorSensor : public sensor::Sensor, public Parented<OFFSRComponent> {
 public:
  ErrorSensor() = default;

 protected:
  // void write_state(sensor::Sensor *error_sensor); // override
  void write_state(float error);
};
*/
	
// /*	
class ErrorSensor : public Component, sensor::Sensor, public Parented<OFFSRComponent> {
 public:
  void dump_config() override;
  void update() override;
  void set_error_sensor(sensor::Sensor *error_sensor) { this->error_sensor_ = error_sensor; };

 protected:
  void write_state(float error);
  sensor::Sensor *error_sensor_{nullptr};
};	
// */
	
 }
}