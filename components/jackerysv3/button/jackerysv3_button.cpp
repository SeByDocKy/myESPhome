#include "jackerysv3_button.h"

#include "esphome/core/log.h"

namespace esphome::jackerysv3 {

static const char *const TAG = "jackerysv3.button";

void JackerySV3RebootButton::dump_config() { LOG_BUTTON("", "Jackery SolarVault 3 Reboot Button", this); }

void JackerySV3RebootButton::press_action() { this->parent_->send_reboot(); }

}  // namespace esphome::jackerysv3
