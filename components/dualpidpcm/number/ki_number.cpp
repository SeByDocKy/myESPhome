#include "esphome/core/version.h"
#include "ki_number.h"

namespace esphome::dualpidpcm {

void KiNumber::setup() {
  float value = this->parent_->get_ki();
  this->publish_state(value);	
}

void KiNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_ki(value);
}

}  // namespace esphome
