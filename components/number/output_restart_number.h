#pragma once

#include "esphome/components/number/number.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class OutputRestartNumber : public number::Number, public Parented<OFFSRComponent> {
 public:
  OutputRestartNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace offsr
}  // namespace esphome
