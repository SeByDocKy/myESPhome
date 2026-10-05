#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "../anker_solix.h"

namespace esphome::anker_solix {

/// Operating mode (register 10064). The n-th option maps to the n-th value added with add_value(); the bit is the
/// one of the EMS mode mask (register 0x8006) that says the device supports the mode.
class AnkerSelect : public select::Select, public Component, public AnkerRegisterItem, public Parented<AnkerSolixHub> {
 public:
  void dump_config() override;
  void add_value(uint16_t value, uint8_t capability_bit) {
    this->values_.push_back(value);
    this->bits_.push_back(capability_bit);
  }
  void parse_and_publish(const std::vector<uint8_t> &data) override;

 protected:
  void control(size_t index) override;

  std::vector<uint16_t> values_;
  std::vector<uint8_t> bits_;
};

}  // namespace esphome::anker_solix
