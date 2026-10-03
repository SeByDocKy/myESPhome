#include "jackerysv3_switch.h"

#include "esphome/core/log.h"

namespace esphome::jackerysv3 {

static const char *const TAG = "jackerysv3.switch";

void JackerySV3Switch::dump_config() {
  LOG_SWITCH("", "Jackery SolarVault 3 Switch", this);
  if (this->kind_ == SwitchKind::PLUG)
    ESP_LOGCONFIG(TAG, "  Plug slot: %u", this->index_);
  else
    ESP_LOGCONFIG(TAG, "  Field: %s", switch_json_field(this->kind_));
}

void JackerySV3Switch::write_state(bool state) {
  bool sent;
  if (this->kind_ == SwitchKind::PLUG)
    sent = this->parent_->send_plug_command(this->index_, state);
  else
    sent = this->parent_->send_main_command(switch_json_field(this->kind_), state ? 1 : 0);
  if (sent)
    this->publish_state(state);  // optimistic; the next report from the battery has the last word
}

}  // namespace esphome::jackerysv3
