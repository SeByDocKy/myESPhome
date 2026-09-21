#include "ez1m_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ez1m {

static const char *const TAG = "ez1m.button";

void EZ1MButton::press_action() {
  // set_power_min -> the inverter's real floor (30 W); set_power_max -> this
  // model's ceiling (800/960/1800 W depending on `model:`). Either way the
  // hub persists the choice to flash and applies it immediately -- see
  // EZ1MComponent::set_startup_power_limit().
  float watts = (this->kind_ == EZ1MButtonType::SET_POWER_MIN) ? 30.0f : this->parent_->get_max_power();
  ESP_LOGD(TAG, "%s pressed: setting startup power limit to %.0f W",
           this->kind_ == EZ1MButtonType::SET_POWER_MIN ? "set_power_min" : "set_power_max", watts);
  this->parent_->set_startup_power_limit(watts);
}

}  // namespace ez1m
}  // namespace esphome
