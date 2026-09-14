#include "output_combined.h"
#include "esphome/core/log.h"

namespace esphome {
namespace output_combined {

static const char *const TAG = "output_combined";

void OutputCombined::dump_config() {
  ESP_LOGCONFIG(TAG, "Output Combined:");
  ESP_LOGCONFIG(TAG, "  Number of outputs: %u", this->outputs_.size());
}

}  // namespace output_combined
}  // namespace esphome
