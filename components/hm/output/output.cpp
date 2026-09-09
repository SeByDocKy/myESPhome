#include "output.h"

namespace esphome {
namespace hm {

void HMPowerLimitPercentOutput::write_state(float state) {
  // La classe de base output::FloatOutput garantit que 'state' est déjà borné à
  // [0.0, 1.0] (en tenant compte de min_power/max_power si configurés côté YAML)
  // avant d'appeler write_state(). On mappe simplement vers 0-100%.
  if (this->parent_ != nullptr) {
    this->parent_->set_power_limit_percent(state * 100.0f);
  }
}

}  // namespace hm
}  // namespace esphome
