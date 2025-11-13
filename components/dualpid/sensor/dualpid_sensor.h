#pragma once

#include "../dualpid.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace dualpid {

class DUALPIDSensor : public Component, public Parented<DUALPIDComponent> {
 public:
  void dump_config() override;
  void setup() override;
  void set_parent(DUALPIDComponent *parent) { this->parent_ = parent; }
  
  void set_error_sensor(sensor::Sensor *sensor) { this->error_sensor_ = sensor; }; 
  void set_output_sensor(sensor::Sensor *sensor) { this->output_sensor_ = sensor; };
  void set_output_charging_sensor(sensor::Sensor *sensor) { this->output_charging_sensor_ = sensor; };
  void set_output_discharging_sensor(sensor::Sensor *sensor) { this->output_discharging_sensor_ = sensor; };
  void set_target_sensor(sensor::Sensor *sensor) { this->target_sensor_ = sensor; }; 

 protected:
  sensor::Sensor *error_sensor_{nullptr};
  sensor::Sensor *output_sensor_{nullptr};
  sensor::Sensor *output_charging_sensor_{nullptr};
  sensor::Sensor *output_discharging_sensor_{nullptr};
  sensor::Sensor *target_sensor_{nullptr};
  DUALPIDComponent *parent_;
  
  void publish_data_();
};	
	
} // dualpid
} // esphome

