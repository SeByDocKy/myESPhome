#include "output_max_number.h"

namespace esphome {
namespace offsr {

void OutputMaxNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_output_max();
  this->parent_->set_output_max(value);
  this->publish_state(value);	
	
}

void OutputMaxNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_output_max(value);
  this->pref_.save(&value);
}

}  // namespace offsr
}  // namespace esphome
