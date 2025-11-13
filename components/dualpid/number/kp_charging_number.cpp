#include "kp_charging_number.h"

namespace esphome {
namespace dualpid {
void KpChargingNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_kp_charging();
  this->parent_->set_kp_charging(value);
  this->publish_state(value);
}

void KpChargingNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_kp_charging(value);
  this->pref_.save(&value);
}

}  // namespace dualpid
}  // namespace esphome

