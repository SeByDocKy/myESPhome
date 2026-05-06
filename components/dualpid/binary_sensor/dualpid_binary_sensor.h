#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../dualpid.h"

namespace esphome {
namespace dualpid {

class DUALPIDBinarySensor : public Component, public Parented<DUALPIDComponent> {
 public:
  void dump_config() override;
  void setup() override;
  
  void set_parent(DUALPIDComponent *parent) { this->parent_ = parent; }
  void set_deadband_binary_sensor(binary_sensor::BinarySensor *bs) { this->deadband_binary_sensor_ = bs; }; 

 protected:
  
  binary_sensor::BinarySensor *deadband_binary_sensor_{nullptr};
  DUALPIDComponent *parent_;
  
  void publish_data_();
};


} // dualpid
} // esphome