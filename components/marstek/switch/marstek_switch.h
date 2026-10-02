#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../marstek.h"

namespace esphome::marstek {

/// Register based switch. The state read back is ON when the register equals the "on" command value; some
/// registers are inverted (backup_function: 0 = on, 1 = off). Nothing is written at boot.
class MarstekSwitch : public switch_::Switch,
                      public Component,
                      public MarstekRegisterItem,
                      public Parented<MarstekHub> {
 public:
  void dump_config() override;
  void set_commands(uint16_t on_value, uint16_t off_value) {
    this->on_value_ = on_value;
    this->off_value_ = off_value;
  }
  void parse_and_publish(const std::vector<uint8_t> &data) override;

 protected:
  void write_state(bool state) override;

  uint16_t on_value_{1};
  uint16_t off_value_{0};
};

}  // namespace esphome::marstek
