#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "../bluetti.h"

namespace esphome::bluetti {

/// Working mode (register 57020). The n-th option maps to the n-th value added with add_value(); the state is read
/// back from the device, a command writes the value of the option (FC 06).
class BluettiSelect : public select::Select, public Component, public BluettiRegisterItem, public Parented<BluettiHub> {
 public:
  void dump_config() override;
  void add_value(uint16_t value) { this->values_.push_back(value); }
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  void control(size_t index) override;

  std::vector<uint16_t> values_;
};

}  // namespace esphome::bluetti
