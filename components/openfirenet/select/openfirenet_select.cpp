#include "openfirenet_select.h"
#ifdef USE_SELECT
#include "esphome/core/log.h"

namespace esphome {
namespace openfirenet {

static const char *const TAG = "openfirenet.select";

void OpenFireNetModeSelect::control(size_t index) {
  // Option order matches the stove's mode numbering: 0 Manual, 1 Auto, 2 Comfort, 3 Setback.
  if (!this->parent_->set_mode(static_cast<long>(index)))
    ESP_LOGW(TAG, "Mode change refused (stove link not ready)");
}

}  // namespace openfirenet
}  // namespace esphome
#endif
