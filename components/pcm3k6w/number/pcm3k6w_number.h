#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/number/number.h"
#include "../pcm3k6w.h"

namespace esphome::pcm3k6w {

// One class handles every number entity; `kind_` (a NumberKind) tells the
// hub which register/setpoint to write. The base number::Number component
// (unlike the `template` platform used in the original YAML) has no
// built-in initial/restored value, so this class restores its last written
// value from flash on boot, falling back to `initial_value_` and pushing
// that out over CAN once.
class PCM3K6WNumber : public number::Number, public Component, public Parented<PCM3K6WComponent> {
 public:
  void setup() override;
  void set_kind(uint8_t kind) { this->kind_ = kind; }
  void set_initial_value(float value) { this->initial_value_ = value; }

 protected:
  void control(float value) override;

  uint8_t kind_{0};
  float initial_value_{0.0f};
  ESPPreferenceObject pref_;
};

}  // namespace esphome::pcm3k6w
