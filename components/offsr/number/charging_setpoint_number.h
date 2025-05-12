#pragma once

#include "esphome/components/number/number.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class ChargingSetpointNumber : public number::Number, public Parented<OFFSRComponent> {
 public:
  // ChargingSetpointNumber() = default;
  void setup();

 protected:
  void control(float value) override;
  OFFSRComponent *parent_;
};

}  // namespace offsr
}  // namespace esphome
