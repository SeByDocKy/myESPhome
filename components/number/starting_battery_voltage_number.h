#pragma once

#include "esphome/components/number/number.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class StartingBatteryVoltageNumber : public number::Number, public Parented<OFFSRComponent> {
 public:
  StartingBatteryVoltageNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace offsr
}  // namespace esphome
