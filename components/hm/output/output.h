#pragma once

#include "esphome/components/output/float_output.h"
#include "../hm.h"

namespace esphome {
namespace hm {

/// Sortie float standard ESPHome (0.0 - 1.0) pilotant la limite de puissance
/// relative du HM. Permet de contrôler l'onduleur depuis n'importe quel
/// composant produisant un output float (PID, template, light, etc.).
class HMPowerLimitPercentOutput : public output::FloatOutput {
 public:
  void set_parent(HMComponent *parent) { this->parent_ = parent; }

 protected:
  void write_state(float state) override;
  HMComponent *parent_{nullptr};
};

}  // namespace hm
}  // namespace esphome
