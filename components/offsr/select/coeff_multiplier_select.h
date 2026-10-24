#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/select/select.h"
#include "../offsr.h"

namespace esphome::offsr {

// Selects the multiplier applied to coeffP, coeffI and coeffD in the PID
// computation ("10", "1", "0.1" or "0.01"). The chosen option index is
// restored from flash on boot (default: "1").
class CoeffMultiplierSelect : public select::Select, public Component, public Parented<OFFSRComponent> {
 public:
  void setup() override;
  void set_initial_index(uint8_t index) { this->initial_index_ = index; }

 protected:
  void control(const std::string &value) override;

  uint8_t initial_index_{0};
  ESPPreferenceObject pref_;
};

}  // namespace esphome::offsr
