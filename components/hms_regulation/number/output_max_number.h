#pragma once

#include "esphome/components/number/number.h"
#include "../hms_regulation.h"

namespace esphome {
namespace hms_regulation {

class OutputMaxNumber : public number::Number, public Component, public Parented<HMS_REGULATIONComponent> {
 public:
  void setup() override;

 protected:
  void control(float value) override;
  ESPPreferenceObject pref_;
};

}  // namespace hms_regulation
}  // namespace esphome
