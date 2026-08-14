#include "esphome/core/version.h"
#include "reverse_switch.h"

namespace esphome::offsr {

void ReverseSwitch::setup() {
  bool state;
  #if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  // this->pref_ = global_preferences->make_preference<bool>(this->get_entity_key());
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  #else
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  #endif
  if (!this->pref_.load(&state)) state = this->parent_->get_reverse();
  this->publish_state(state);
  this->parent_->set_reverse(state);
}

void ReverseSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_reverse(state);
  this->pref_.save(&state);
}

}  // namespace esphome::offsr



