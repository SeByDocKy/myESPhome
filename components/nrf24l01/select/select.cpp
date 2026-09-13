#include "select.h"

namespace esphome {
namespace nrf24l01 {

void NRF24PALevelSelect::control(const std::string &value) {
  if (this->parent_ == nullptr) return;

  uint8_t level;
  if (value == "min") {
    level = 0;
  } else if (value == "low") {
    level = 1;
  } else if (value == "high") {
    level = 2;
  } else {
    level = 3;  // "max"
  }

  // apply_pa_level_runtime() republie déjà l'état exact sur ce select --
  // pas besoin de publish_state() ici en plus.
  this->parent_->apply_pa_level_runtime(level);
}

}  // namespace nrf24l01
}  // namespace esphome
