#pragma once

#include "esphome/components/number/number.h"
#include "../offsr.h"

namespace esphome {
namespace osr {

class ChargingSetpointNumber : public number::Number, public Parented<OFFSRComponent> {
 public:
  ChargingSetpointNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace offsr
}  // namespace esphome
