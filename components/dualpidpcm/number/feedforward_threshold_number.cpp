#include "feedforward_threshold_number.h"

namespace esphome {
namespace dualpidpcm {

void FeedforwardthresholdNumber::setup() {	
	float value;
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	if (!this->pref_.load(&value)) value = this->parent_->get_feedforward_threshold();
	this->parent_->set_feedforward_threshold(value);
	this->publish_state(value);
}

void FeedforwardthresholdNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_feedforward_threshold(value);
  this->pref_.save(&value);
}

}  // namespace dualpidpcm
}  // namespace esphome
