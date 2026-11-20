#include "rylr998_number.h"
#include "../rylr998.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

namespace esphome::rylr998 {

static const char *const TAG = "rylr998.number";

void RYLR998TxPowerNumber::setup() {
  if (this->restore_value_) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 7, 0)
    this->pref_ = this->make_entity_preference<uint8_t>();
#else
    this->pref_ = global_preferences->make_preference<uint8_t>(this->get_object_id_hash());
#endif
    uint8_t saved;
    if (this->pref_.load(&saved)) {
      // apply_tx_power() clamps to the module's maximum and the module is configured later, in
      // the parent's loop(): nothing is sent from here.
      this->parent_->apply_tx_power(saved);
    }
  }
  // The real (clamped) value is published by the parent once the module confirms it; until then
  // show what will be sent.
  this->publish_state(static_cast<float>(this->parent_->get_tx_power()));
}

void RYLR998TxPowerNumber::dump_config() {
  LOG_NUMBER("", "RYLR998 TX power", this);
  ESP_LOGCONFIG(TAG, "  Restore value: %s", YESNO(this->restore_value_));
}

void RYLR998TxPowerNumber::control(float value) {
  const uint8_t power = static_cast<uint8_t>(value);
  ESP_LOGD(TAG, "TX power control: %u dBm requested", (unsigned) power);
  this->parent_->apply_tx_power(power);  // non-blocking, clamped to the variant's maximum
  if (this->restore_value_) {
    const uint8_t actual = this->parent_->get_tx_power();
    this->pref_.save(&actual);
  }
  this->publish_state(static_cast<float>(this->parent_->get_tx_power()));  // show the clamped value
}

}  // namespace esphome::rylr998
