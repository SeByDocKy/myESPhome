#include "esphome/core/version.h"
#include "self_consumption_number.h"

namespace esphome::dualpidpcm {

void SelfConsumptionNumber::setup() {	
	float value;
	#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
    // this->pref_ = global_preferences->make_preference<float>(this->get_entity_key());
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    #else
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	#endif
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
