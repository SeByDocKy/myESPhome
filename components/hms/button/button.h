#pragma once

#include "esphome/components/button/button.h"
#include "../hms.h"

namespace esphome {
namespace hms {

/// Generic button: on press, forces the HMS relative power limit to
/// a fixed target value (used for reset_to_output_min / reset_to_output_max).
class HMSResetPercentButton : public button::Button {
 public:
  void set_parent(HMSComponent *parent) { this->parent_ = parent; }
  void set_target_percent(float percent) { this->target_percent_ = percent; }

 protected:
  void press_action() override;
  HMSComponent *parent_{nullptr};
  float target_percent_{100.0f};
};

/// On press, hardware resets the CMT2300A chip (without rebooting the ESP32)
/// followed by a full Hoymiles reconfiguration. Used as a fallback if the
/// inverter never becomes reachable after an ESP32 boot -- see the docs for an
/// automation that presses it periodically while reachable stays off.
class HMSResetHmsButton : public button::Button {
 public:
  void set_parent(HMSComponent *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  HMSComponent *parent_{nullptr};
};

}  // namespace hms
}  // namespace esphome
