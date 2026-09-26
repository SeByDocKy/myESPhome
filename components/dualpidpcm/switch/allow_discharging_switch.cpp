#include "esphome/core/version.h"
#include "allow_discharging_switch.h"

namespace esphome::dualpidpcm {
void AllowDischargingSwitch::setup() {
  bool state;
  #if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  //this->pref_ = global_preferences->make_preference<bool>(this->get_entity_key());
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  #else
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  #endif
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
