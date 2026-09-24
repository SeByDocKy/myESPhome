#include "output.h"

namespace esphome {
namespace hmsw {

void HMSWPersistentPowerPercentOutput::write_state(float state) {
  // output::FloatOutput guarantees 'state' is already clamped to [0.0, 1.0]
  // (accounting for min_power/max_power if set in YAML) before this is
  // called. Map it to 0-100%.
  if (this->parent_ != nullptr) {
    this->parent_->set_persistent_power_limit_percent(state * 100.0f);
  }
}

}  // namespace hmsw
}  // namespace esphome
