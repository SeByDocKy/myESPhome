#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/button/button.h"
#include "esphome/components/apszb/apszb.h"

namespace esphome {
namespace apszb {

class APSButton : public button::Button, public Parented<APSInverter> {
 public:
  // 0 = pair, 1 = refresh (poll now), 2 = query max power, 3 = reboot the inverter
  void set_action(uint8_t action) { this->action_ = action; }

 protected:
  void press_action() override {
    switch (this->action_) {
      case 0:
        this->parent_->request_pair();
        break;
      case 1:
        this->parent_->request_refresh();
        break;
      case 2:
        this->parent_->request_query();
        break;
      default:
        this->parent_->request_reboot();
        break;
    }
  }
  uint8_t action_{0};
};

}  // namespace apszb
}  // namespace esphome
