#pragma once

#include "esphome/components/button/button.h"
#include "../ez1m.h"

namespace esphome {
namespace ez1m {

enum class EZ1MButtonType {
  SET_POWER_MIN,
  SET_POWER_MAX,
};

class EZ1MButton : public button::Button, public Parented<EZ1MComponent> {
 public:
  void set_kind(EZ1MButtonType kind) { this->kind_ = kind; }

 protected:
  void press_action() override;

  EZ1MButtonType kind_{EZ1MButtonType::SET_POWER_MIN};
};

}  // namespace ez1m
}  // namespace esphome
