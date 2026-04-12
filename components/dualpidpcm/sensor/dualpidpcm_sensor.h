#pragma once

#include "../dualpidpcm.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace dualpidpcm {

class DUALPIDPCMSensor : public Component, public Parented<DUALPIDPCMComponent> {
 public:
  void dump_config() override;
  void setup() override;
  void set_parent(DUALPIDPCMComponent *parent) { this->parent_ = parent; }
  void set_error_sensor(sensor::Sensor *sensor) { this->error_sensor_ = sensor; }
  void set_output_charging_sensor(sensor::Sensor *sensor) { this->output_charging_sensor_ = sensor; }
  void set_output_discharging_sensor(sensor::Sensor *sensor) { this->output_discharging_sensor_ = sensor; }
  void set_input_sensor(sensor::Sensor *sensor) { this->input_sensor_ = sensor; }

 protected:
  sensor::Sensor *error_sensor_{nullptr};
  sensor::Sensor *output_charging_sensor_{nullptr};
  sensor::Sensor *output_discharging_sensor_{nullptr};
  sensor::Sensor *input_sensor_{nullptr};

  DUALPIDPCMComponent *parent_;
  
  void publish_data_();
};	
	
} // dualpidpcm
} // esphome


