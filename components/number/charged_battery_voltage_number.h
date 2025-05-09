#pragma once

#include "esphome/components/number/number.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class ChargedBatteryVoltageNumber : public number::Number, public Parented<OFFSRComponent> {
 public:
  ChargedBatteryVoltageNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace offsr
}  // namespace esphome
