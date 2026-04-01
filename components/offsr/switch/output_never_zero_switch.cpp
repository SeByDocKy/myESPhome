#include "activation_switch.h"

namespace esphome {
namespace offsr {
void OutputNeverZeroSwitch::setup() {
  bool state;
  this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
  if (!this->pref_.load(&state)) state = this->parent_->get_output_never_zero();
  this->publish_state(state);
  this->parent_->set_output_never_zero(state);
}

void OutputNeverZeroSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_output_never_zero(state);
  this->pref_.save(&state);
}

}  // namespace offsr
}  // namespace esphome
