#include "esphome/core/version.h"
#include "output_min_number.h"

namespace esphome::offsr {

void OutputMinNumber::setup() {
  float value;
  #if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  // this->pref_ = global_preferences->make_preference<float>(this->get_entity_key());
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  #else
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  #endif
  if (!this->pref_.load(&value)) value = this->parent_->get_output_min();
  this->parent_->set_output_min(value);
  this->publish_state(value);
}

void OutputMinNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_output_min(value);
  this->pref_.save(&value);
}

}  // namespace esphome::offsr
