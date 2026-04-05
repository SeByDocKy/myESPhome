#pragma once
#include "esphome/components/number/number.h"
#include "../dualpidpcm.h"

namespace esphome {
namespace dualpidpcm {

class OutputMinNumber : public number::Number, public Component, public Parented<DUALPIDPCMComponent> {
 public:
  void setup() override;

 protected:
  void control(float value) override;
  ESPPreferenceObject pref_;
};

}  // namespace dualpidpcm
}  // namespace esphome
