#include "floating_epoint_number.h"

namespace esphome {
namespace dualpid {

void FloatingEpointNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_floating_epoint();
  this->parent_->set_floating_epoint(value*0.01);
  this->publish_state(value);	
}


void FloatingEpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_floating_epoint(value*0.01);
  this->pref_.save(&value);
}

}  // namespace dualpid
}  // namespace esphome

