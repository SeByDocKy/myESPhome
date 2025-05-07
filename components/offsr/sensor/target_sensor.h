#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {
	
/*
class OutputSensor : public sensor::Sensor, public Parented<OFFSRComponent> {
 public:
  OutputSensor() = default;

 protected:
  // void write_state(sensor::Sensor *output_sensor); // override
  void write_state(float output);
};	
*/
	
// /*	
class TargetSensor : public Component, sensor::Sensor, public Parented<OFFSRComponent> {
 public:
  void dump_config() override;
  // void update(); //override
  void setup() override;
  // void set_parent(OFFSRComponent *parent) { parent_ = parent; }

  void set_target_sensor(sensor::Sensor *target_sensor) { this->target_sensor_ = target_sensor; };

 protected:
  sensor::Sensor *target_sensor_{nullptr};
  OFFSRComponent *parent_;
  // void update_from_parent_();
};	
// */
	
 }
}