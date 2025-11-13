#include "activation_switch.h"

namespace esphome {
namespace dualpid {
void ActivationSwitch::setup() {
  bool state;
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  if (!this->pref_.load(&state)) state = this->parent_->get_activation();
  this->publish_state(state);
  this->parent_->set_activation(state);
}

void ActivationSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_activation(state);
  this->pref_.save(&state);
}

}  // namespace dualpid
}  // namespace esphome
