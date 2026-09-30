#include "esphome/core/version.h"
#include "kp_number.h"

namespace esphome::dualpidpcm {

void KpNumber::setup() {
  float value = this->parent_->get_kp();
  this->publish_state(value);
}

void KpNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_kp(value);
}

}  // namespace esphome

