#include "esphome/core/version.h"
#include "pid_mode_switch.h"

namespace esphome::dualpid {
	
void PidModeSwitch::setup() {
  bool state;
  #if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  // this->pref_ = global_preferences->make_preference<bool>(this->get_entity_key());
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  #else
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  #endif
  if (!this->pref_.load(&state)) state = this->parent_->get_pid_mode();
  this->parent_->set_pid_mode(state);
  this->publish_state(state);
}
void PidModeSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_pid_mode(state);
  this->pref_.save(&state);
}

}  // namespace esphome

