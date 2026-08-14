#include "esphome/core/version.h"
#include "charging_epoint_number.h"

namespace esphome::dualpid {

void ChargingEpointNumber::setup() {	
	float value;
	#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
    // this->pref_ = global_preferences->make_preference<float>(this->get_entity_key());
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    #else
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	#endif
	if (!this->pref_.load(&value)) value = this->parent_->get_charging_epoint();
	this->parent_->set_charging_epoint(value*0.01f);
	this->publish_state(value);
}

void ChargingEpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_charging_epoint(value*0.01f);
  this->pref_.save(&value);
}

}  // namespace esphome








