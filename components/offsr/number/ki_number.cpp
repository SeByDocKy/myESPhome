#include "ki_number.h"

namespace esphome {
namespace offsr {
void KiNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_ki();
  this->parent_->set_ki(value);
  this->publish_state(value);	
}


void KiNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_ki(value);
  this->pref_.save(&value);
}

}  // namespace offsr
}  // namespace esphome
