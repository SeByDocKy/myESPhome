#include "output_max_number.h"

namespace esphome {
namespace minipid {

void OutputMaxNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_output_max();
  this->publish_state(value*0.01);	
  this->parent_->set_output_max(value);
}

void OutputMaxNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_output_max(value*0.01);
  this->pref_.save(&value);
}

}  // namespace minipid
}  // namespace esphome


