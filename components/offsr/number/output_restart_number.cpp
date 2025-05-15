#include "output_restart_number.h"

namespace esphome {
namespace offsr {

void OutputRestartNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_output_restart();
  this->publish_state(value);	
	
}

void OutputRestartNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_output_restart(value);
}

}  // namespace offsr
}  // namespace esphome
