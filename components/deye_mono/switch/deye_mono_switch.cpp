#include "deye_mono_switch.h"

namespace esphome::deye_mono {

static const char *const TAG = "deye_mono.switch";

void DeyeMonoSwitch::dump_config() {
  LOG_SWITCH("", "Deye Switch", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  mask: 0x%04X", this->reg_, this->mask_);
}

void DeyeMonoSwitch::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  const uint16_t word = this->word_(data);
  // Ignored while the register is held after a write (the poll may have been under way before the write)
  if (!this->parent_->accept_read(this->reg_, word))
    return;
  this->publish_state((word & this->mask_) != 0);
}

void DeyeMonoSwitch::write_state(bool state) {
  if (!this->parent_->write_masked(this->reg_, this->mask_, state ? this->mask_ : 0)) {
    this->publish_state(this->state);  // put the switch back to what the inverter has
    return;
  }
  this->publish_state(state);
}

}  // namespace esphome::deye_mono
