#pragma once

#include "esphome/components/switch/switch.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {
class OFFSRSwitch : public Component, public Parented<OFFSRComponent> {
 public:
  // void dump_config() override;
  // void setup() override;
  // void update() override;
  
  void set_activation_switch(switch_::Switch *s) { this->activation_switch_ = s; }; 
  void set_manual_override_switch(switch_::Switch *s) { this->manual_override_switch_ = s; }; 

 protected:
  
  switch_::Switch *activation_switch_;
  switch_::Switch *manual_override_switch_;
  OFFSRComponent *parent_;
  
  void write_state(bool state); //  override
  
};

} // offsr
} // esphome