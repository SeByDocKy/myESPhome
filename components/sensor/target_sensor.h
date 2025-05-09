#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {
		
class TargetSensor : public Component, public sensor::Sensor, public Parented<OFFSRComponent> {
 public:
  void dump_config() override;
  // void update(); //override
  void setup() override;

/*   void set_target_sensor(sensor::Sensor *target_sensor) { this->target_sensor_ = target_sensor; }; */

 protected:
/*   sensor::Sensor *target_sensor_{nullptr}; */
  OFFSRComponent *parent_;
};	
	
 }
}