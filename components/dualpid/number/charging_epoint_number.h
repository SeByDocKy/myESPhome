#pragma once
#include "esphome/components/number/number.h"
#include "../dualpid.h"

namespace esphome {
namespace dualpid {

class ChargingEpointNumber : public number::Number, public Component, public Parented<DUALPIDComponent> {
 public:
  void setup() override;

 protected:
  void control(float value) override;
  ESPPreferenceObject pref_;
};

}  // namespace dualpid
}  // namespace esphome

