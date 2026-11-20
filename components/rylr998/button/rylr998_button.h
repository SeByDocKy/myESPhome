#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"

namespace esphome::rylr998 {

class RYLR998Component;

/// Restarts the radio module (AT+RESET). The configuration is re-applied automatically once the
/// module reports +READY.
class RYLR998ResetButton : public button::Button, public Parented<RYLR998Component> {
 protected:
  void press_action() override;
};

}  // namespace esphome::rylr998
