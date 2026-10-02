#pragma once

#include "esphome/components/button/button.h"
#include "../marstek.h"

namespace esphome::marstek {

/// Writes a fixed command value to a register when pressed (reset device).
class MarstekButton : public button::Button, public Parented<MarstekHub> {
 public:
  void set_command(uint16_t reg, uint16_t command) {
    this->reg_ = reg;
    this->command_ = command;
  }

 protected:
  void press_action() override;

  uint16_t reg_{0};
  uint16_t command_{0};
};

}  // namespace esphome::marstek
