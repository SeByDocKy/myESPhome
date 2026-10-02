#include "marstek_button.h"

namespace esphome::marstek {

static const char *const TAG = "marstek.button";

void MarstekButton::press_action() {
  ESP_LOGI(TAG, "'%s': writing %u to register %u", this->get_name().c_str(), this->command_, this->reg_);
  this->parent_->write_register(this->reg_, this->command_);
}

}  // namespace esphome::marstek
