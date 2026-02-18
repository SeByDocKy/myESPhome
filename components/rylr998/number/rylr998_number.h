#pragma once

#include "esphome/components/number/number.h"

namespace esphome {
namespace rylr998 {

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

}  // namespace rylr998
}  // namespace esphome
