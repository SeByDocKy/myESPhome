#include "button.h"

namespace esphome {
namespace hmsw {

void HMSWResetButton::press_action() {
  if (this->parent_ != nullptr) {
    this->parent_->reboot_dtu();
  }
}

}  // namespace hmsw
}  // namespace esphome
