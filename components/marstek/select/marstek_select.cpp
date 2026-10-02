#include "marstek_select.h"

namespace esphome::marstek {

static const char *const TAG = "marstek.select";

void MarstekSelect::dump_config() {
  LOG_SELECT("", "Marstek Select", this);
  ESP_LOGCONFIG(TAG, "  Register: %u  skip_updates: %u", this->reg_, this->skip_updates);
}

void MarstekSelect::parse_and_publish(const std::vector<uint8_t> &data) {
  if (!this->has_data_(data))
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

void MarstekSelect::control(size_t index) {
  if (index >= this->values_.size())
    return;
  this->parent_->write_register(this->reg_, this->values_[index]);
  this->publish_state(index);
}

}  // namespace esphome::marstek
