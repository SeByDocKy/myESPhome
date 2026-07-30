#include "allow_discharging_switch.h"

namespace esphome::dualpidpcm {
void AllowDischargingSwitch::setup() {
  bool state;
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  if (!this->pref_.load(&state)) state = this->parent_->get_allow_discharging();
  this->publish_state(state);
  this->parent_->set_allow_discharging(state);
}

void AllowDischargingSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_allow_discharging(state);
  this->pref_.save(&state);
}
}  // namespace esphome
