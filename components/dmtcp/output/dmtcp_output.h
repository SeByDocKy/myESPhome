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
  uint8_t write_unit_id_ = 0x02;
  uint8_t write_fcn_code_= 0x06;
  uint16_t start_modbus_address_ = 0x0028;
  uint16_t nb_bytes_to_read_ = 0x0001;
};	
	
	
 }
}