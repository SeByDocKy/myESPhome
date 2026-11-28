#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "../ecoflow_stream.h"

#include <vector>

namespace esphome::ecoflow_stream {

class EcoflowStreamSelect : public select::Select,
                            public Component,
                            public EcoflowStreamRegisterItem,
                            public Parented<EcoflowStreamHub> {
 public:
  void setup() override { this->writer_.init(this->parent_->controller()); }
  void dump_config() override;
  void add_value(uint16_t value) { this->values_.push_back(value); }
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  void control(size_t index) override;

  std::vector<uint16_t> values_;
  EcoflowStreamWriter writer_;
};

}  // namespace esphome::ecoflow_stream
