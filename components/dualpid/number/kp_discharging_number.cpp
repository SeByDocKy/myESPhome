#include "kp_discharging_number.h"

namespace esphome {
namespace dualpid {
void KpDischargingNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_kp_discharging();
  this->parent_->set_kp_discharging(value);
  this->publish_state(value);
}

void KpDischargingNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_kp_discharging(value);
  this->pref_.save(&value);
}

}  // namespace dualpid
}  // namespace esphome

