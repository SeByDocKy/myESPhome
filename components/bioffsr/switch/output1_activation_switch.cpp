#include "output1_activation_switch.h"

namespace esphome {
namespace bioffsr {
	
void Output1ActivationSwitch::setup() {
  bool state;
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  if (!this->pref_.load(&state)) state = this->parent_->get_output1_activation();
  this->publish_state(state);
  this->parent_->set_output1_activation(state);
}

void Output1ActivationSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_output1_activation(state);
  this->pref_.save(&state);
}

}  // namespace bioffsr
}  // namespace esphome


