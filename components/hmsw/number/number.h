#pragma once

#include "esphome/components/number/number.h"
#include "../hmsw.h"

namespace esphome {
namespace hmsw {

class HMSWPersistentPowerPercentNumber : public number::Number {
 public:
  void set_parent(HMSWComponent *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override;
  HMSWComponent *parent_{nullptr};
};

}  // namespace hmsw
}  // namespace esphome
