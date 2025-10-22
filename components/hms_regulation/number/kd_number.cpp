#include "kd_number.h"

namespace esphome {
namespace hms_regulation {

void KdNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_kd();
  this->parent_->set_kd(value);
  this->publish_state(value);
}

void KdNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_kd(value);
  this->pref_.save(&value);
}

}  // namespace hms_regulation
}  // namespace esphome
