#pragma once

#include "esphome/components/number/number.h"
#include "../hm.h"

namespace esphome {
namespace hm {

class HMPowerPercentNumber : public number::Number {
 public:
  void set_parent(HMComponent *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override;
  HMComponent *parent_{nullptr};
};

class HMPowerAbsoluteNumber : public number::Number {
 public:
  void set_parent(HMComponent *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override;
  HMComponent *parent_{nullptr};
};

}  // namespace hm
}  // namespace esphome
