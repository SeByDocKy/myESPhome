#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/version.h"

namespace esphome::rylr998 {
// Forward declaration du composant parent
class RYLR998Component;


class RYLR998TxPowerNumber : public number::Number, public Component {
 public:
  void setup() override;
  void set_parent(RYLR998Component *parent) { this->parent_ = parent; }

 protected:
 
  void control(float value) override;
  RYLR998Component *parent_{nullptr};
  ESPPreferenceObject pref_;
};

}  // namespace esphome::rylr998
