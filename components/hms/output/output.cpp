#include "output.h"

namespace esphome {
namespace hms {

void HMSPowerLimitPercentOutput::write_state(float state) {
  // The output::FloatOutput base class guarantees 'state' is already clamped to
  // [0.0, 1.0] (accounting for min_power/max_power if set in YAML)
  // before calling write_state(). We simply map it to 0-100%.
  if (this->parent_ != nullptr) {
    this->parent_->set_power_limit_percent(state * 100.0f);
  }
}

}  // namespace hms
}  // namespace esphome
