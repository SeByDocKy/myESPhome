#include "esphome/core/version.h"
#include "delta_idle_charging_number.h"

namespace esphome::dualpidpcm {

void DeltaIdleChargingNumber::setup() {	
	float value;
	#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
    // this->pref_ = global_preferences->make_preference<float>(this->get_entity_key());
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    #else
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	#endif
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
