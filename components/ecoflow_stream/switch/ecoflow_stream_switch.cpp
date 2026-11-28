#include "ecoflow_stream_switch.h"

namespace esphome::ecoflow_stream {

static const char *const TAG = "ecoflow_stream.switch";

void EcoflowStreamSwitch::dump_config() {
  LOG_SWITCH("", "EcoFlow STREAM Switch", this);
  ESP_LOGCONFIG(TAG, "  Register: %u", this->reg_);
}

void EcoflowStreamSwitch::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data) || this->held_())
    return;
  this->publish_state(this->word_(data, 0) != 0);
}

void EcoflowStreamSwitch::write_state(bool state) {
  if (!this->writer_.write_single_register(this->reg_, state ? 1 : 0)) {
    ESP_LOGW(TAG, "Write of register %u refused by the Modbus bus (queue full?)", this->reg_);
    return;
  }
  this->hold_for(WRITE_HOLD_MS);
  this->publish_state(state);
}

void EcoflowStreamHeartbeatSwitch::setup() {
  const bool state = this->get_initial_state_with_restore_mode().value_or(true);
  this->write_state(state);
}

void EcoflowStreamHeartbeatSwitch::dump_config() { LOG_SWITCH("", "EcoFlow STREAM Heartbeat", this); }

void EcoflowStreamHeartbeatSwitch::write_state(bool state) {
  this->parent_->set_heartbeat_enabled(state);
  this->publish_state(state);
}

}  // namespace esphome::ecoflow_stream
