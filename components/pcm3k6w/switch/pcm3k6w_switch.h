#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../pcm3k6w.h"

namespace esphome::pcm3k6w {

// One class handles every switch entity; `kind_` (a SwitchKind) tells the
// hub which physical control to drive. This mirrors the ld2420-style
// "single class, dispatch by kind" pattern rather than one subclass per
// entity, since there are only a handful of writable registers here.
class PCM3K6WSwitch : public switch_::Switch, public Component, public Parented<PCM3K6WComponent> {
 public:
  void set_kind(uint8_t kind) { this->kind_ = kind; }

 protected:
  void write_state(bool state) override;

  uint8_t kind_{0};
};

}  // namespace esphome::pcm3k6w
