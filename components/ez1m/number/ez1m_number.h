#pragma once

#include "esphome/components/number/number.h"
#include "../ez1m.h"

namespace esphome {
namespace ez1m {

class EZ1MNumber : public number::Number, public Parented<EZ1MComponent> {
 public:
  void set_kind(EZ1MNumberType kind) { this->kind_ = kind; }

 protected:
  void control(float value) override;

  EZ1MNumberType kind_;
};

}  // namespace ez1m
}  // namespace esphome
