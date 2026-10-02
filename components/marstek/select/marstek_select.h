#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "../marstek.h"

namespace esphome::marstek {

/// Register holding one of a fixed set of values (user work mode, force mode, grid standard). The n-th option
/// of the select maps to the n-th value added with add_value().
class MarstekSelect : public select::Select,
                      public Component,
                      public MarstekRegisterItem,
                      public Parented<MarstekHub> {
 public:
  void dump_config() override;
  void add_value(uint16_t value) { this->values_.push_back(value); }
  void parse_and_publish(const std::vector<uint8_t> &data) override;

 protected:
  void control(size_t index) override;

  std::vector<uint16_t> values_;
};

}  // namespace esphome::marstek
