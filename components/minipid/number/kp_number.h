#pragma once

#include "esphome/components/number/number.h"
#include "../minipid.h"

namespace esphome {
namespace minipid {

class KpNumber : public number::Number, public Component, public Parented<MINIPIDComponent> {
 public:
  void setup() override;

 protected:
  void control(float value) override;
  ESPPreferenceObject pref_;
};

}  // namespace minipid
}  // namespace esphome
