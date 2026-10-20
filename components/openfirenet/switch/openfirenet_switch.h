#pragma once

#include "esphome/core/defines.h"
#ifdef USE_SWITCH
#include "esphome/core/helpers.h"
#include "esphome/components/switch/switch.h"
#include "../openfirenet.h"

namespace esphome {
namespace openfirenet {

class OpenFireNetPowerSwitch : public switch_::Switch, public Component, public Parented<OpenFireNetComponent> {
 public:
  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace openfirenet
}  // namespace esphome
#endif
