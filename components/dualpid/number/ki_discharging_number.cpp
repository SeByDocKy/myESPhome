#include "ki_discharging_number.h"

namespace esphome {
namespace dualpid {
void KiDischargingNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_ki_discharging();
  this->parent_->set_ki_discharging(value);
  this->publish_state(value);	
}

void KiDischargingNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_ki_discharging(value);
  this->pref_.save(&value);
}

}  // namespace offsr
}  // namespace esphome

