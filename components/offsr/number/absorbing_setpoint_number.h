#pragma once

#include "esphome/components/number/number.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class AbsorbingSetpointNumber : public number::Number, public Parented<OFFSRComponent> {
 public:
  AbsorbingSetpointNumber() = default;
  void setup() override;

 protected:
  void control(float value) override;
};

}  // namespace offsr
}  // namespace esphome
