#pragma once

#include "esphome/components/output/float_output.h"
#include "../hmsw.h"

namespace esphome {
namespace hmsw {

/// Standard ESPHome float output (0.0 - 1.0) driving the HMS-XXXXW's
/// relative power limit. Named "persistent": unlike hm:/hms:, no
/// non-persistent (RAM-only) variant of this command is known for
/// HMS-XXXXW -- every write hits the inverter's EEPROM. See README.md
/// before wiring this into a fast control loop (PID, etc.).
class HMSWPersistentPowerLimitPercentOutput : public output::FloatOutput {
 public:
  void set_parent(HMSWComponent *parent) { this->parent_ = parent; }

 protected:
  void write_state(float state) override;
  HMSWComponent *parent_{nullptr};
};

}  // namespace hmsw
}  // namespace esphome
