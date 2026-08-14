#include "esphome/core/version.h"
#include "kd_number.h"

namespace esphome::dualpidpcm {

void KdNumber::setup() {
  float value;
  #if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
  // this->pref_ = global_preferences->make_preference<float>(this->get_entity_key());
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  #else
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  #endif
  if (!this->pref_.load(&value)) value = this->parent_->get_kd();
  this->parent_->set_kd(value);
  this->publish_state(value);
}

void KdNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_kd(value);
  this->pref_.save(&value);
}

}  // namespace esphome
