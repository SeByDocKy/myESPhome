#include "emerson_r48_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace emerson_r48 {


static const char *const TAG = "emerson_r48";

void EmersonR48Button::press_action() { 
    ESP_LOGD(TAG, "-> button pressed");
    this->parent_->set_offline_values(); 
}

}  // namespace emerson_r48
}  // namespace esphome
