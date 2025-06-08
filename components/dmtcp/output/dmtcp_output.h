#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"
#include "../dmtcp.h"

namespace esphome {
namespace dmtcp {
	
class DMTCPOutput : public Component, public output::FloatOutput, public Parented<DMTCPComponent> {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA - 1; }
  void write_state(float state) override;

 protected:
  // uint8_t channel_;
};	
	
	
 }
}