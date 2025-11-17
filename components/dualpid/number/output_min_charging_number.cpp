#include "output_min_charging_number.h"

namespace esphome {
namespace dualpid {

void OutputMinChargingNumber::setup() {
  float value;
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  if (!this->pref_.load(&value)) value = this->parent_->get_output_min_charging();
  this->parent_->set_output_min_charging(value);
  this->publish_state(value*0.01f);
}

void OutputMinChargingNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_output_min_charging(value*0.01f);
  this->pref_.save(&value);
}

}  // namespace dualpid
}  // namespace esphome









