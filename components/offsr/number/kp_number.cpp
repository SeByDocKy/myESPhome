#include "kp_number.h"

namespace esphome {
namespace offsr {
void KpNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_kp();
  this->publish_state(value);
}


void KpNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_kp(value);
}

}  // namespace offsr
}  // namespace esphome
