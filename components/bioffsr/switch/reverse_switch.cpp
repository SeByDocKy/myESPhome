#include "reverse_switch.h"

namespace esphome {
namespace bioffsr {
	
void ReverseSwitch::setup() {
  bool state;
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  if (!this->pref_.load(&state)) state = this->parent_->get_reverse();
  this->publish_state(state);
  this->parent_->set_reverse(state);
}

void ReverseSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_reverse(state);
  this->pref_.save(&state);
}

}  // namespace bioffsr
}  // namespace esphome


