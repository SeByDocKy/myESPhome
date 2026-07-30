#include "allow_charging_switch.h"

namespace esphome::dualpidpcm {
void AllowChargingSwitch::setup() {
  bool state;
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  if (!this->pref_.load(&state)) state = this->parent_->get_allow_charging();
  this->publish_state(state);
  this->parent_->set_allow_charging(state);
}

void AllowChargingSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_allow_charging(state);
  this->pref_.save(&state);
}
}  // namespace esphome
