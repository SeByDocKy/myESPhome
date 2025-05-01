#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {
	
class OutputSensor : public sensor::Sensor, public Parented<OFFSRComponent> {
 public:
  OutputSensor() = default;

 protected:
  void write_state(sensor::Sensor *output_sensor); // override
};	
	
/*	
class OutputSensor : public Component, sensor::Sensor {
 public:
  void dump_config() override;
  void set_output_sensor(sensor::Sensor *output_sensor) { this->output_sensor_ = output_sensor; };

 protected:
  sensor::Sensor *output_sensor_{nullptr};
};	
*/
	
 }
}