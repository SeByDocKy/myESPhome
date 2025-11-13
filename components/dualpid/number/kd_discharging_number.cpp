#include "kd_discharging_number.h"

namespace esphome {
namespace dualpid {

void KdDischargingNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_kd_discharging();
  this->parent_->set_kd_discharging(value);
  this->publish_state(value);
}

void KdDischargingNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_kd_discharging(value);
  this->pref_.save(&value);
}

}  // namespace dualpid
}  // namespace esphome
