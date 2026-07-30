#include "self_consumption_number.h"

namespace esphome::dualpidpcm {

void SelfConsumptionNumber::setup() {	
	float value;
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	if (!this->pref_.load(&value)) value = this->parent_->get_self_consumption();
	this->parent_->set_self_consumption(value);
	this->publish_state(value);
}

void SelfConsumptionNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_self_consumption(value);
  this->pref_.save(&value);
}

}  // namespace esphome
