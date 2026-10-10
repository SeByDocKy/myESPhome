#include "deye_mono_select.h"

namespace esphome::deye_mono {

static const char *const TAG = "deye_mono.select";

void DeyeMonoSelect::dump_config() {
  LOG_SELECT("", "Deye Select", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  mask: 0x%04X", this->reg_, this->mask_);
}

void DeyeMonoSelect::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data))
    return;
  const uint16_t word = this->word_(data);
  // Ignored while the register is held after a write (the poll may have been under way before the write)
  if (!this->parent_->accept_read(this->reg_, word))
    return;
  const uint16_t value = word & this->mask_;
  for (size_t i = 0; i < this->values_.size(); i++) {
    if (this->values_[i] == value) {
      if (!this->has_state() || this->active_index() != i)
        this->publish_state(i);
      return;
    }
  }
  ESP_LOGD(TAG, "Register %u holds %u, which is not one of the options of '%s'", this->reg_, value,
           this->get_name().c_str());
}

void DeyeMonoSelect::control(size_t index) {
  if (index >= this->values_.size())
    return;
  if (!this->parent_->write_masked(this->reg_, this->mask_, this->values_[index])) {
    // Show the option the inverter really has
    if (this->has_state())
      this->publish_state(this->active_index().value());
    return;
  }
  this->publish_state(index);
}

}  // namespace esphome::deye_mono
