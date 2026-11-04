#include "deyelvhy_switch.h"

#include "esphome/core/defines.h"

#ifdef USE_SWITCH

namespace esphome::deyelvhy {

static const char *const TAG = "deyelvhy.switch";

void DeyeLvHySwitch::dump_config() {
  LOG_SWITCH("", "Deye Switch", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  mask: 0x%04X", this->reg_, this->mask_);
}

void DeyeLvHySwitch::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  const uint16_t word = this->word_(data);
  // Ignored while the register is held after a write (the poll may have been under way before the write)
  if (!this->parent_->accept_read(this->reg_, word))
    return;
  this->publish_state((word & this->mask_) != 0);
}

void DeyeLvHySwitch::write_state(bool state) {
  if (!this->parent_->write_masked(this->reg_, this->mask_, state ? this->mask_ : 0)) {
    this->publish_state(this->state);  // put the switch back to what the inverter has
    return;
  }
  this->publish_state(state);
}

}  // namespace esphome::deyelvhy

#endif  // USE_SWITCH
