#include "coeff_multiplier_select.h"

namespace esphome::offsr {

void CoeffMultiplierSelect::setup() {
  this->pref_ = global_preferences->make_preference<uint8_t>(this->get_object_id_hash());
  uint8_t index;
  if (!this->pref_.load(&index)) index = this->initial_index_;

  const auto &options = this->traits.get_options();
  if (index >= options.size()) index = 0;
  if (options.empty()) return;

  const std::string &value = options[index];
  this->parent_->set_coeff_multiplier(strtof(value.c_str(), nullptr));
  this->publish_state(value);
}

void CoeffMultiplierSelect::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->set_coeff_multiplier(strtof(value.c_str(), nullptr));

  const auto &options = this->traits.get_options();
  for (size_t i = 0; i < options.size(); i++) {
    if (options[i] == value) {
      uint8_t index = static_cast<uint8_t>(i);
      this->pref_.save(&index);
      break;
    }
  }
}

}  // namespace esphome::offsr
