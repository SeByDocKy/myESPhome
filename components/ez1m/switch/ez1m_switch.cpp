#include "ez1m_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ez1m {

static const char *const TAG = "ez1m.switch";

void EZ1MSwitch::setup() {
  // Optimistic default UI state on boot; the hub will correct it as soon as
  // it parses the inverter's real state byte from the first status frame.
  bool initial = this->get_initial_state_with_restore_mode().value_or(true);
  this->publish_state(initial);
}

void EZ1MSwitch::write_state(bool state) {
  this->publish_state(state);
  if (state) {
    // Re-use the current power limit setting if we have one, else the hub
    // falls back to 800 W (full power), matching the original YAML behavior.
    this->parent_->turn_on(this->parent_->get_power_limit_value());
  } else {
    this->parent_->turn_off();
  }
}

void EZ1MSwitch::dump_config() { LOG_SWITCH("", "EZ1M Inverter On/Off", this); }

}  // namespace ez1m
}  // namespace esphome
