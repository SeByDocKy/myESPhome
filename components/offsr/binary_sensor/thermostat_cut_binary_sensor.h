#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class ThermostatcutBinarySensor : public Component, public binary_sensor::BinarySensor, public Parented<OFFSRComponent> {
 public:
  void dump_config() override;
  void set_parent(OFFSRComponent *parent) { parent_ = parent; }
  // void update();
  
  void setup() override;
  
/*   void set_thermostat_cut_binary_sensor(binary_sensor::BinarySensor *thermostat_cut_binary_sensor) { this->thermostat_cut_binary_sensor_ = thermostat_cut_binary_sensor; }; */
 
 protected:
/*   binary_sensor::BinarySensor *thermostat_cut_binary_sensor_{nullptr}; */
  OFFSRComponent *parent_;
};	
 }
}