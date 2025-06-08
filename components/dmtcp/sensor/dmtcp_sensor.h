#pragma once

#include "../dmtcp.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace dmtcp {

class DMTCPSensor : public Component, public Parented<DMTCPComponent> {
 public:
  void dump_config() override;
  void setup() override;
  void set_parent(DMTCPComponent *parent) { this->parent_ = parent; }
  
  void set_pv1_voltage_sensor(sensor::Sensor *sensor) { this->pv1_voltage_ = sensor; }; 


 protected:
  sensor::Sensor *pv1_voltage_{nullptr};
  DMTCPComponent *parent_;
  
  void publish_data_();
};	
	
} // dmtcp
} // esphome
