#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class OFFSRBinarySensor : public Component, public Parented<OFFSRComponent> {
 public:
  void dump_config() override;
  void setup() override;
  
  void set_parent(OFFSRComponent *parent) { this->parent_ = parent; }
  void set_thermostat_cut_binary_sensor(binary_sensor::BinarySensor *thermostat_cut_binary_sensor) { this->thermostat_cut_binary_sensor_ = thermostat_cut_binary_sensor; }; 

 protected:
  binary_sensor::BinarySensor *thermostat_cut_binary_sensor_{nullptr};
  OFFSRComponent *parent_;
  
  void publish_data_();
};


} // offsr
} // esphome