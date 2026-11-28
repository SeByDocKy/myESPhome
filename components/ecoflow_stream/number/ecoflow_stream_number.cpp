#include "ecoflow_stream_number.h"

#include <algorithm>

namespace esphome::ecoflow_stream {

static const char *const TAG = "ecoflow_stream.number";

void EcoflowStreamNumber::dump_config() {
  LOG_NUMBER("", "EcoFlow STREAM Number", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  count: %u", this->reg_, this->count_);
}

void EcoflowStreamNumber::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data) || this->held_())
    return;
  const float value = this->number_(data);
  this->publish_state(value == 0.0f ? 0.0f : value * this->sign_factor_());
}

void EcoflowStreamNumber::control(float value) {
  if (std::isnan(value))
    return;
  if (this->system_hub_ != nullptr && !this->system_hub_->system_valid()) {
    ESP_LOGW(TAG, "Register %u not written: this unit is not (yet) known as the system master", this->reg_);
    return;
  }
  const float limited = std::min(std::max(value, this->traits.get_min_value()), this->traits.get_max_value());
  const int32_t integer = static_cast<int32_t>(std::lround(limited));
  // `integer` is what the entity shows, `wire` what the device expects
  int32_t wire = integer * this->sign_factor_();
  if (this->sign_ != nullptr)
    wire = std::min<int32_t>(std::max<int32_t>(wire, INT16_MIN), INT16_MAX);
  ESP_LOGD(TAG, "Write register %u = %d", this->reg_, static_cast<int>(wire));
  bool queued;
  if (this->count_ == 2) {
    const uint16_t words[2] = {static_cast<uint16_t>(static_cast<uint32_t>(wire) >> 16),
                               static_cast<uint16_t>(static_cast<uint32_t>(wire) & 0xFFFF)};
    queued = this->writer_.write_multiple_registers(this->reg_, std::span<const uint16_t>(words));
  } else {
    queued = this->writer_.write_single_register(this->reg_, static_cast<uint16_t>(wire));
  }
  if (!queued) {
    ESP_LOGW(TAG, "Write of register %u refused by the Modbus bus (queue full?)", this->reg_);
    return;
  }
  this->hold_for(WRITE_HOLD_MS);
  this->publish_state(static_cast<float>(integer));
}

}  // namespace esphome::ecoflow_stream
