#include "ecoflow_stream_select.h"

namespace esphome::ecoflow_stream {

static const char *const TAG = "ecoflow_stream.select";

void EcoflowStreamSelect::dump_config() {
  LOG_SELECT("", "EcoFlow STREAM Select", this);
  ESP_LOGCONFIG(TAG, "  Register: %u", this->reg_);
}

void EcoflowStreamSelect::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data) || this->held_())
    return;
  const uint16_t value = this->word_(data, 0);
  for (size_t i = 0; i < this->values_.size(); i++) {
    if (this->values_[i] == value) {
      this->publish_state(i);
      return;
    }
  }
  ESP_LOGD(TAG, "Register %u holds %u, which is not one of the options", this->reg_, value);
}

void EcoflowStreamSelect::control(size_t index) {
  if (index >= this->values_.size())
    return;
  if (this->system_hub_ != nullptr && !this->system_hub_->system_valid()) {
    ESP_LOGW(TAG, "Register %u not written: this unit is not (yet) known as the system master", this->reg_);
    return;
  }
  if (!this->writer_.write_single_register(this->reg_, this->values_[index])) {
    ESP_LOGW(TAG, "Write of register %u refused by the Modbus bus (queue full?)", this->reg_);
    return;
  }
  this->hold_for(WRITE_HOLD_MS);
  this->publish_state(index);
}

}  // namespace esphome::ecoflow_stream
