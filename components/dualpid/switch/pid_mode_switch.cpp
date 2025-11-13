#include "pid_mode_switch.h"

namespace esphome {
namespace offsr {
	
void PidModeSwitch::setup() {
  bool state;
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  if (!this->pref_.load(&state)) state = this->parent_->get_pid_mode();
  this->parent_->set_pid_mode(state);
  this->publish_state(state);
}
void PidModeSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_pid_mode(state);
  this->pref_.save(&state);
}

}  // namespace offsr
}  // namespace esphome
