#include "manual_level_number.h"

namespace esphome {
namespace offsr {

void ManualLevelNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_manual_level(value);
}

}  // namespace offsr
}  // namespace esphome
