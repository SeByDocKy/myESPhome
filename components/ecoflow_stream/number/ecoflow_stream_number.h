#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "../ecoflow_stream.h"

namespace esphome::ecoflow_stream {

/// Writable register: UINT16 / INT16 (FC 0x06) or UINT32 (FC 0x10, high word first)
class EcoflowStreamNumber : public number::Number,
                            public Component,
                            public EcoflowStreamRegisterItem,
                            public Parented<EcoflowStreamHub> {
 public:
  void setup() override { this->writer_.init(this->parent_->controller()); }
  void dump_config() override;
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  void control(float value) override;

  EcoflowStreamWriter writer_;
};

}  // namespace esphome::ecoflow_stream
