#include "ez1m_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ez1m {

static const char *const TAG = "ez1m.number";

void EZ1MNumber::control(float value) {
  switch (this->kind_) {
    case EZ1MNumberType::POWER_LIMIT:
      // Non-optimistic: the UI only updates once the inverter echoes the new
      // limit back in its next status frame (see EZ1MComponent::handle_frame_).
      this->parent_->set_power_limit(value);
      break;
    case EZ1MNumberType::TOTAL_ENERGY:
      // Optimistic manual override/reset of the lifetime energy counter.
      this->publish_state(value);
      this->parent_->set_lifetime_energy(value);
      break;
  }
}

}  // namespace ez1m
}  // namespace esphome
