#include "openfirenet_number.h"
#ifdef USE_NUMBER
#include "esphome/core/log.h"

namespace esphome {
namespace openfirenet {

static const char *const TAG = "openfirenet.number";

void OpenFireNetTargetTemperatureNumber::control(float value) {
  if (!this->parent_->set_target_temperature(value))
    ESP_LOGW(TAG, "Target temperature refused (stove link not ready)");
}

void OpenFireNetHeatingPowerNumber::control(float value) {
  if (!this->parent_->set_heating_power(value))
    ESP_LOGW(TAG, "Heating power refused (stove link not ready)");
}

}  // namespace openfirenet
}  // namespace esphome
#endif
