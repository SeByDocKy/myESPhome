#include "esphome/core/version.h"
#include "feedforward_threshold_number.h"

namespace esphome::dualpidpcm {
void FeedforwardthresholdNumber::setup() {	
	float value;
	#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
    // this->pref_ = global_preferences->make_preference<float>(this->get_entity_key());
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    #else
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	#endif
	if (!this->pref_.load(&value)) value = this->parent_->get_feedforward_threshold();
	this->parent_->set_feedforward_threshold(value);
	this->publish_state(value);
}

void FeedforwardthresholdNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_feedforward_threshold(value);
  this->pref_.save(&value);
}

}  // namespace esphome
