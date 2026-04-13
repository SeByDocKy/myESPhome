#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../dualpidpcm.h"

namespace esphome {
namespace dualpidpcm {

class DUALPIDPCMBinarySensor : public Component, public Parented<DUALPIDPCMComponent> {
 public:
  void dump_config() override;
  void setup() override;
  
  void set_parent(DUALPIDPCMComponent *parent) { this->parent_ = parent; }
  void set_deadband_binary_sensor(binary_sensor::BinarySensor *bs) { this->deadband_binary_sensor_ = bs; }
  void set_swap_binary_sensor(binary_sensor::BinarySensor *bs) { this->swap_binary_sensor_ = bs; }

 protected:
  
  binary_sensor::BinarySensor *deadband_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *swap_binary_sensor_{nullptr};
  DUALPIDPCMComponent *parent_;
  
  void publish_data_();
};

} // dualpidpcm
} // esphome
