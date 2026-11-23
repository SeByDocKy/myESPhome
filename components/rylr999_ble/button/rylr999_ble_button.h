#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"

namespace esphome::rylr999_ble {

class RYLR999BLEComponent;

/// Restarts the radio module (AT+RESET). The configuration is re-applied automatically once the
/// module reports +READY.
class RYLR999BLEResetButton : public button::Button, public Parented<RYLR999BLEComponent> {
 protected:
  void press_action() override;
};

}  // namespace esphome::rylr999_ble
