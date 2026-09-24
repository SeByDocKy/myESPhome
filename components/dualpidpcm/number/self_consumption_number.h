#pragma once

#include "esphome/components/number/number.h"
#include "../dualpidpcm.h"

namespace esphome::dualpidpcm {

class SelfConsumptionNumber : public number::Number, public Component, public Parented<DUALPIDPCMComponent> {
 public:
  void setup() override;

 protected:
  void control(float value) override;
  ESPPreferenceObject pref_;
};

}  // namespace esphome::dualpidpcm

