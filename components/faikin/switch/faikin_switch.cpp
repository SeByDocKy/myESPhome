#include "faikin_switch.h"

namespace esphome {
namespace faikin {

void FaikinSwitch::write_state(bool state) {
  // On success the hub publishes the new state once the unit acknowledged the write. On refusal
  // nothing is published, so the frontend keeps showing the unit's real value.
  this->parent_->control_flag(this->flag_, state);
}

}  // namespace faikin
}  // namespace esphome
