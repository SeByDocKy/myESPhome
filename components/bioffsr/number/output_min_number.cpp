#include "output_min_number.h"

namespace esphome {
namespace bioffsr {

void OutputMinNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_output_min();
  this->parent_->set_output_min(value);
  this->publish_state(value);
}

void OutputMinNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_output_min(value);
  this->pref_.save(&value);
}

}  // namespace bioffsr
}  // namespace esphome
