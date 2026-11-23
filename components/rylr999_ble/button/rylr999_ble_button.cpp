#include "rylr999_ble_button.h"
#include "../rylr999_ble.h"
#include "esphome/core/log.h"

namespace esphome::rylr999_ble {

static const char *const TAG = "rylr999_ble.button";

void RYLR999BLEResetButton::press_action() {
  ESP_LOGD(TAG, "Reset requested");
  this->parent_->request_reset();  // non-blocking
}

}  // namespace esphome::rylr999_ble
