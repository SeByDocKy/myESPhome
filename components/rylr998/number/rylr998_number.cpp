#include "rylr998_number.h"
#include "../rylr998.h"
#include "esphome/core/log.h"

namespace esphome::rylr998 {

static const char *const TAG = "rylr998.number";

void RYLR998TxPowerNumber::setup() {

  uint8_t value;
  #if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
  this->pref_ = global_preferences->make_preference<uint8_t>(this->get_entity_key());
  #else
  this->pref_ = global_preferences->make_preference<uint8_t>(this->get_object_id_hash());
  #endif
  if (!this->pref_.load(&value)) value = this->parent_->get_tx_power();
  this->parent_->apply_tx_power(value);
  this->publish_state(value);
}


void RYLR998TxPowerNumber::control(float value) {
  // value est déjà clampée par ESPHome entre min_value et max_value
  uint8_t power = static_cast<uint8_t>(value);
  ESP_LOGD(TAG, "TX power control: %.0f dBm requested", value);
  this->parent_->apply_tx_power(power);
  this->publish_state(value);
}

}  // namespace eesphome::rylr998
