#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/select/select.h"
#include "../pcm3k6w.h"

namespace esphome::pcm3k6w {

// One class handles every select entity; `kind_` (a SelectKind) tells the
// hub which mode/setting to write. Like PCM3K6WNumber, this restores its
// last chosen option index from flash on boot (select::Select has no
// built-in restore, unlike the `template` platform used in the original
// YAML) and pushes it out over CAN once.
class PCM3K6WSelect : public select::Select, public Component, public Parented<PCM3K6WComponent> {
 public:
  void setup() override;
  void set_kind(uint8_t kind) { this->kind_ = kind; }
  void set_initial_index(uint8_t index) { this->initial_index_ = index; }

 protected:
  void control(const std::string &value) override;

  uint8_t kind_{0};
  uint8_t initial_index_{0};
  ESPPreferenceObject pref_;
};

}  // namespace esphome::pcm3k6w
