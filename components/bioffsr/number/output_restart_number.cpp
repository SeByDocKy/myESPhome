#include "output_restart_number.h"

namespace esphome {
namespace bioffsr {

void OutputRestartNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_output_restart();
  this->parent_->set_output_restart(value);
  this->publish_state(value);		
}

void OutputRestartNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_output_restart(value);
  this->pref_.save(&value);
}

}  // namespace bioffsr
}  // namespace esphome
