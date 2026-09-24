#include "esphome/core/version.h"
#include "timer_standby_poweroff_number.h"

namespace esphome::dualpidpcm {

void TimerStandbyPoweroffNumber::setup() {
	float value;
	#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	#else
	this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
	#endif
	if (!this->pref_.load(&value)) value = this->parent_->get_timer_standby_poweroff();
	this->parent_->set_timer_standby_poweroff(value);
	this->publish_state(value);
}

void TimerStandbyPoweroffNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_timer_standby_poweroff(value);
  this->pref_.save(&value);
}

}  // namespace esphome::dualpidpcm
