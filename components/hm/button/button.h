#pragma once

#include "esphome/components/button/button.h"
#include "../hm.h"

namespace esphome {
namespace hm {

/// Generic button: on press, forces the HM's relative power limit to
/// a fixed target value (used for reset_to_output_min / reset_to_output_max),
/// written PERSISTENT (inverter EEPROM).
class HMResetPercentButton : public button::Button {
 public:
  void set_parent(HMComponent *parent) { this->parent_ = parent; }
  void set_target_percent(float percent) { this->target_percent_ = percent; }

 protected:
  void press_action() override;
  HMComponent *parent_{nullptr};
  float target_percent_{100.0f};
};

/// On press, hardware resets the nRF24L01 chip (without rebooting the ESP32)
/// followed by a full Hoymiles NRF reconfiguration.
class HMResetHmButton : public button::Button {
 public:
  void set_parent(HMComponent *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  HMComponent *parent_{nullptr};
};

}  // namespace hm
}  // namespace esphome
