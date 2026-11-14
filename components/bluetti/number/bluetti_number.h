#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "../bluetti.h"

namespace esphome::bluetti {

/// State of charge limit (registers 57016 / 57017, %): the value is read back from the device, a command writes
/// the rounded value (FC 06). The limits of the number (5..100 %) are the ones of the register library.
class BluettiNumber : public number::Number, public Component, public BluettiRegisterItem, public Parented<BluettiHub> {
 public:
  void dump_config() override;
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  void control(float value) override;
};

}  // namespace esphome::bluetti
