#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class ErrorSensor : public Component, public sensor::Sensor, public Parented<OFFSRComponent> {
 public:
  void dump_config() override;
  // void update(); //override
  void setup() override;
  void set_parent(OFFSRComponent *parent) { parent_ = parent; }
  
 /*  void set_error_sensor(sensor::Sensor *error_sensor) { this->error_sensor_ = error_sensor; }; */

 protected:
/*   sensor::Sensor *error_sensor_{nullptr}; */
  OFFSRComponent *parent_;
  
};	
	
 }
}