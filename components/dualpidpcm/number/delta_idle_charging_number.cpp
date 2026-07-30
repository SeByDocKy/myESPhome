#include "delta_idle_charging_number.h"

namespace esphome::dualpidpcm {

void DeltaIdleChargingNumber::setup() {	
	float value;
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	if (!this->pref_.load(&value)) value = this->parent_->get_delta_idle_charging();
	this->parent_->set_delta_idle_charging(value);
	this->publish_state(value);
}

void DeltaIdleChargingNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_delta_idle_charging(value);
  this->pref_.save(&value);
}
}  // namespace esphome
