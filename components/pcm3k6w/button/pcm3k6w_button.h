#pragma once

#include "esphome/components/button/button.h"
#include "../pcm3k6w.h"

namespace esphome::pcm3k6w {

// One class handles every button entity; `kind_` (a ButtonKind) tells the
// hub which momentary command to send. button::Button has no state and no
// Component base, so unlike switch/number/select this is a plain trigger:
// the hub doesn't need a pointer back to it.
class PCM3K6WButton : public button::Button, public Parented<PCM3K6WComponent> {
 public:
  void set_kind(uint8_t kind) { this->kind_ = kind; }

 protected:
  void press_action() override;

  uint8_t kind_{0};
};

}  // namespace esphome::pcm3k6w
