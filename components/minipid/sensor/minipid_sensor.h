#pragma once

#include "../minipid.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace minipid {

class MINIPIDSensor : public Component, public Parented<MINIPIDComponent> {
 public:
  void dump_config() override;
  void setup() override;
  void set_parent(MINIPIDComponent *parent) { this->parent_ = parent; }
  
  void set_error_sensor(sensor::Sensor *sensor) { this->error_sensor_ = sensor; }; 
  void set_output_sensor(sensor::Sensor *sensor) { this->output_sensor_ = sensor; }; 
  void set_target_sensor(sensor::Sensor *sensor) { this->target_sensor_ = sensor; }; 

 protected:
  sensor::Sensor *error_sensor_{nullptr};
  sensor::Sensor *output_sensor_{nullptr};
  sensor::Sensor *target_sensor_{nullptr};
  MINIPIDComponent *parent_;
  
  void publish_data_();
};	
	
} // minipid
} // esphome
