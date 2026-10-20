#pragma once

#include "esphome/core/defines.h"
#ifdef USE_NUMBER
#include "esphome/core/helpers.h"
#include "esphome/components/number/number.h"
#include "../openfirenet.h"

namespace esphome {
namespace openfirenet {

class OpenFireNetTargetTemperatureNumber : public number::Number, public Parented<OpenFireNetComponent> {
 protected:
  void control(float value) override;
};

class OpenFireNetHeatingPowerNumber : public number::Number, public Parented<OpenFireNetComponent> {
 protected:
  void control(float value) override;
};

}  // namespace openfirenet
}  // namespace esphome
#endif
