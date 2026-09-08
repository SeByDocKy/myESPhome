#pragma once

#include "esphome/components/number/number.h"
#include "../hms.h"

namespace esphome {
namespace hms {

class HMSPowerPercentNumber : public number::Number {
 public:
  void set_parent(HMSComponent *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override;
  HMSComponent *parent_{nullptr};
};

class HMSPowerAbsoluteNumber : public number::Number {
 public:
  void set_parent(HMSComponent *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override;
  HMSComponent *parent_{nullptr};
};

}  // namespace hms
}  // namespace esphome
