#include "rylr998_button.h"
#include "../rylr998.h"
#include "esphome/core/log.h"

namespace esphome::rylr998 {

static const char *const TAG = "rylr998.button";

void RYLR998ResetButton::press_action() {
  ESP_LOGD(TAG, "Reset requested");
  this->parent_->request_reset();  // non-blocking
}

}  // namespace esphome::rylr998
