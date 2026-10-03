#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"
#include "../jackerysv3.h"

namespace esphome::jackerysv3 {

/// Reboot the battery (`{"cmd":5,"rc":1,"reboot":1}`)
class JackerySV3RebootButton : public button::Button, public Component, public Parented<JackerySV3Hub> {
 public:
  void dump_config() override;

 protected:
  void press_action() override;
};

}  // namespace esphome::jackerysv3
