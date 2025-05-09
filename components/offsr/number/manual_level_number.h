#pragma once

#include "esphome/components/number/number.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class ManualLevelNumber : public number::Number, public Parented<OFFSRComponent> {
 public:
  ManualLevelNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace offsr
}  // namespace esphome
