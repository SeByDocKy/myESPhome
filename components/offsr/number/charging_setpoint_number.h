#pragma once

#include "esphome/components/number/number.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class ChargingSetpointNumber : public number::Number, public Parented<OFFSRComponent> {
 public:
  // ChargingSetpointNumber() = default;
  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;
  // OFFSRComponent *parent_;
   ESPPreferenceObject pref_;
};

}  // namespace offsr
}  // namespace esphome
