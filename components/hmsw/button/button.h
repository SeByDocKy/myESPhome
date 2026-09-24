#pragma once

#include "esphome/components/button/button.h"
#include "../hmsw.h"

namespace esphome {
namespace hmsw {

// Reboots the DTU/inverter's own network stack -- NOT the ESP32 this
// component runs on. See HMSWComponent::reboot_dtu() and README.md.
class HMSWResetButton : public button::Button {
 public:
  void set_parent(HMSWComponent *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  HMSWComponent *parent_{nullptr};
};

}  // namespace hmsw
}  // namespace esphome
