#include "output_max_number.h"

namespace esphome {
namespace minipid {

void OutputMaxNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_output_max()*1.0f;  // should be in [0-100%]
  this->parent_->set_output_max(value);
  this->publish_state(value*100.0f);	


  // if (!this->pref_.load(&value)) value = this->parent_->get_output_max();  // should be in [0-100%]
  // this->parent_->set_output_max(value*0.01);
  // this->publish_state(value);	
}

void OutputMaxNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_output_max(value*0.01);
  this->pref_.save(&value);
}

}  // namespace minipid
}  // namespace esphome







