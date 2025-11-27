#pragma once

#include "esphome/components/number/number.h"
#include "../bioffsr.h"

namespace esphome {
namespace bioffsr {

class OutputRestartNumber : public number::Number, public Component, public Parented<BIOFFSRComponent> {
 public:
  void setup() override;

 protected:
  void control(float value) override;
  ESPPreferenceObject pref_;
};

}  // namespace bioffsr
}  // namespace esphome
