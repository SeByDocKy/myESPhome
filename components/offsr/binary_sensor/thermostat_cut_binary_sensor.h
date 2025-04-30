#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {
	
class ThermostatcutBinarySensor : public binary_sensor::BinarySensor, public Parented<OFFSRComponent> {
 public:
  ThermostatcutBinarySensor() = default;

 protected:
  void write_state(binary_sensor::BinarySensor *thermostat_cut_binary_sensor) override;
};	
/*	
class ThermostatcutBinarySensor : public Component, binary_sensor::BinarySensor {
 public:
  void dump_config() override;
  void set_thermostat_cut_binary_sensor(binary_sensor::BinarySensor *thermostat_cut_binary_sensor) { this->thermostat_cut_binary_sensor_ = thermostat_cut_binary_sensor; };
 
 protected:
  binary_sensor::BinarySensor *thermostat_cut_binary_sensor_{nullptr};
};	
	
*/	
	
 }
}