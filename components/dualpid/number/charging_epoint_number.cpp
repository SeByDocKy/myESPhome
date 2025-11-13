#include "charging_epoint_number.h"

namespace esphome {
namespace duapid {

void ChargingEpointNumber::setup() {	
	float value;
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	if (!this->pref_.load(&value)) value = this->parent_->get_charging_epoint();
	this->parent_->set_charging_epoint(value);
	this->publish_state(value);
}

void ChargingEpointNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_charging_epoint(value);
  this->pref_.save(&value);
}

}  // namespace dualpid
}  // namespace esphome

