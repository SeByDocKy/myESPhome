#include "rylr998_number.h"
#include "../rylr998.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rylr998 {

static const char *const TAG = "rylr998.number";

void RYLR998TxPowerNumber::control(float value) {
  // value est déjà clampée par ESPHome entre min_value et max_value
  uint8_t power = static_cast<uint8_t>(value);

  ESP_LOGD(TAG, "TX power control: %.0f dBm requested", value);

  // Envoie AT+CRFOP=<power> et met à jour tx_power_ dans le composant
  this->parent_->apply_tx_power(power);

  // Publie la nouvelle valeur dans Home Assistant
  this->publish_state(value);
}

}  // namespace rylr998
}  // namespace esphome
