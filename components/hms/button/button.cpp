#include "button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hms {

static const char *const TAG = "hms.button";

void HMSResetPercentButton::press_action() {
  if (this->parent_ == nullptr) return;
  ESP_LOGW(TAG, "Bouton pressé -- écriture PERSISTANTE (EEPROM onduleur) de la limite à %.1f%%",
           this->target_percent_);
  this->parent_->set_power_limit_percent_persistent(this->target_percent_);
}

void HMSResetHmsButton::press_action() {
  if (this->parent_ == nullptr) return;
  this->parent_->reset_radio();
}

}  // namespace hms
}  // namespace esphome
