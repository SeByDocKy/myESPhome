#include "output2_activation_switch.h"

namespace esphome {
namespace bioffsr {
	
void Output2ActivationSwitch::setup() {
  bool state;
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  if (!this->pref_.load(&state)) state = this->parent_->get_output2_activation();
  this->publish_state(state);
  this->parent_->set_output2_activation(state);
}

void Output2ActivationSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_output2_activation(state);
  this->pref_.save(&state);
}

}  // namespace bioffsr
}  // namespace esphome


