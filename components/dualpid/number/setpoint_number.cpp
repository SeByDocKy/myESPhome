#include "setpoint_number.h"

namespace esphome {
namespace dualpid {

void SetpointNumber::setup() {	
	float value;
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	if (!this->pref_.load(&value)) value = this->parent_->get_setpoint();
	this->parent_->set_setpoint(value);
	this->publish_state(value);
}

void SetpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_setpoint(value);
  this->pref_.save(&value);
}

}  // namespace dualpid
}  // namespace esphome
