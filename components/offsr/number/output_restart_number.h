#pragma once

#include "esphome/components/number/number.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class OutputRestartNumber : public number::Number, public Component, public Parented<OFFSRComponent> {
 public:
  

 protected:
  void control(float value) override;
  ESPPreferenceObject pref_;
};

}  // namespace offsr
}  // namespace esphome
