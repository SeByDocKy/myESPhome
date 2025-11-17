#include "r48_switch.h"

namespace esphome {
namespace dualpid {
void R48Switch::setup() {
  bool state;
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  if (!this->pref_.load(&state)) state = this->parent_->get_r48();
  this->publish_state(state);
  this->parent_->set_r48(state);
}

void R48Switch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_r48(state);
  this->pref_.save(&state);
}

}  // namespace dualpid
}  // namespace esphome

