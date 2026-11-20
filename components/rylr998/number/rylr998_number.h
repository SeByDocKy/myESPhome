#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

namespace esphome::rylr998 {

class RYLR998Component;

/// TX power (AT+CRFOP) as a number entity.
/// The value is only persisted across reboots when `restore_value: true` is set in the YAML.
class RYLR998TxPowerNumber : public number::Number, public Component, public Parented<RYLR998Component> {
 public:
  void setup() override;
  void dump_config() override;
  // Must run before the parent's loop() sends the configuration: the restored value has to be
  // known when AT+CRFOP is sent.
  float get_setup_priority() const override { return setup_priority::DATA - 1.0f; }
  void set_restore_value(bool restore) { this->restore_value_ = restore; }

 protected:
  void control(float value) override;
  bool restore_value_{false};
  ESPPreferenceObject pref_;
};

}  // namespace esphome::rylr998
