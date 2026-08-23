#include "pcm3k6w_select.h"

namespace esphome::pcm3k6w {

void PCM3K6WSelect::setup() {
  this->pref_ = global_preferences->make_preference<uint8_t>(this->get_object_id_hash());
  uint8_t index;
  if (!this->pref_.load(&index)) index = this->initial_index_;

  const auto &options = this->traits.get_options();
  if (index >= options.size()) index = 0;
  std::string value = options.empty() ? std::string("") : options[index];

  this->publish_state(value);
  if (!value.empty()) this->parent_->write_select(this->kind_, value);
}

void PCM3K6WSelect::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->write_select(this->kind_, value);

  const auto &options = this->traits.get_options();
  for (size_t i = 0; i < options.size(); i++) {
    if (options[i] == value) {
      uint8_t index = static_cast<uint8_t>(i);
      this->pref_.save(&index);
      break;
    }
  }
}

}  // namespace esphome::pcm3k6w
