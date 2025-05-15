#include "manual_override_switch.h"

namespace esphome {
namespace offsr {

void ManualOverrideSwitch::setup() {
  bool state;
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  if (!this->pref_.load(&state)) state = this->parent_->get_manual_override();
  this->publish_state(state);	
}

void ManualOverrideSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_manual_override(state);
  this->pref_.save(&state);
}

}  // namespace offsr
}  // namespace esphome
