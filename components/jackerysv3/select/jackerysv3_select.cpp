#include "jackerysv3_select.h"

#include "esphome/core/log.h"

namespace esphome::jackerysv3 {

static const char *const TAG = "jackerysv3.select";

void JackerySV3Select::dump_config() {
  LOG_SELECT("", "Jackery SolarVault 3 Select", this);
  ESP_LOGCONFIG(TAG, "  Field: %s", select_json_field(this->kind_));
}

void JackerySV3Select::control(size_t index) {
  if (this->parent_->send_main_command(select_json_field(this->kind_), static_cast<int>(index)))
    this->publish_state(index);  // optimistic; the next report from the battery has the last word
}

}  // namespace esphome::jackerysv3
