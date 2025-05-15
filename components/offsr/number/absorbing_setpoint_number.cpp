#include "absorbing_setpoint_number.h"

namespace esphome {
namespace offsr {

void AbsorbingSetpointNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_absorbing_setpoint();
  this->publish_state(value);
	
}

void AbsorbingSetpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_absorbing_setpoint(value);
  this->pref_.save(&value);
}

}  // namespace offsr
}  // namespace esphome
