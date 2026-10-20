#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/number/number.h"
#include "esphome/components/faikin/faikin.h"

namespace esphome {
namespace faikin {

/// Demand control limit in percent (100 = no limit). The hub publishes the value read back from the unit.
class FaikinNumber : public number::Number, public Parented<FaikinComponent> {
 protected:
  void control(float value) override;
};

}  // namespace faikin
}  // namespace esphome
