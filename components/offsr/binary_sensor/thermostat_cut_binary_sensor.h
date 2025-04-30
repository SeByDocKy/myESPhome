#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {
class ThermostatcutBinarySensor : public Component, binary_sensor::BinarySensor {
 public:
  void dump_config() override;
  void set_thermostat_cut_binary_sensor(binary_sensor::BinarySensor *thermostat_cut_binary_sensor) { this->thermostat_cut_binary_sensor_ = thermostat_cut_binary_sensor; };
/*  
  void on_presence(bool presence) override {
    if (this->presence_bsensor_ != nullptr) {
      if (this->presence_bsensor_->state != presence)
        this->presence_bsensor_->publish_state(presence);
    }
  }
*/ 

 protected:
  binary_sensor::BinarySensor *thermostat_cut_binary_sensor_{nullptr};
};	
	
	
	
 }
}