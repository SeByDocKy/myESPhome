#pragma once

#include "esphome/components/number/number.h"
#include "../cmt2300a.h"

namespace esphome {
namespace cmt2300a {

/// Réglage de la puissance d'émission à la volée (-10 à +20 dBm), sans reset de
/// la puce -- set_pa_level() n'écrit que 2-3 registres directement, sûr à
/// appeler à tout moment, y compris pendant qu'un hms: partage la même radio.
class CMT2300APALevelNumber : public number::Number {
 public:
  void set_parent(CMT2300AComponent *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override;
  CMT2300AComponent *parent_{nullptr};
};

}  // namespace cmt2300a
}  // namespace esphome
