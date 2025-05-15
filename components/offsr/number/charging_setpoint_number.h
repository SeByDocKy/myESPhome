#pragma once

#include "esphome/components/number/number.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class ChargingSetpointNumber : public number::Number, public Component, public Parented<OFFSRComponent> {
 public:
  void setup() override;
  // void dump_config() override;

 protected:
  void control(float value) override;
  ESPPreferenceObject pref_;
};

}  // namespace offsr
}  // namespace esphome
