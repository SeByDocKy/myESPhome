#include "openfirenet_switch.h"
#ifdef USE_SWITCH
#include "esphome/core/log.h"

namespace esphome {
namespace openfirenet {

static const char *const TAG = "openfirenet.switch";

void OpenFireNetPowerSwitch::dump_config() { LOG_SWITCH("", "OpenFireNet Power Switch", this); }

void OpenFireNetPowerSwitch::write_state(bool state) {
  // The state is published by the hub (optimistically right after the write, then from the stove).
  if (!this->parent_->set_power(state))
    ESP_LOGW(TAG, "Power command refused (stove link not ready)");
}

}  // namespace openfirenet
}  // namespace esphome
#endif
