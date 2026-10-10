#include "anker_solix_select.h"

namespace esphome::anker_solix {

static const char *const TAG = "anker_solix.select";

// After a write the battery needs a few seconds to apply the command: the values read back meanwhile are old
static constexpr uint32_t WRITE_HOLD_MS = 15000;

void AnkerSolixSelect::dump_config() {
  LOG_SELECT("", "Anker SOLIX Select", this);
  ESP_LOGCONFIG(TAG, "  Register: %u", this->reg_);
}

void AnkerSolixSelect::parse_and_publish(std::span<const uint8_t> data) {
  if (!this->has_data_(data) || this->held_())
    return;
  const uint16_t value = this->word_(data, 0);
  for (size_t i = 0; i < this->values_.size(); i++) {
    if (this->values_[i] == value) {
      this->publish_state(i);
      return;
    }
  }
  ESP_LOGD(TAG, "Register %u holds %u, which is not one of the options of '%s'", this->reg_, value,
           this->get_name().c_str());
}

void AnkerSolixSelect::control(size_t index) {
  if (index >= this->values_.size())
    return;
  if (!this->parent_->request_operating_mode(this->values_[index], this->bits_[index])) {
    // The device does not support this mode: show the mode it is really in
    if (this->has_state())
      this->publish_state(this->active_index().value());
    return;
  }
  this->hold_for(WRITE_HOLD_MS);
  this->publish_state(index);
}

}  // namespace esphome::anker_solix
