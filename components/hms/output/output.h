#pragma once

#include "esphome/components/output/float_output.h"
#include "../hms.h"

namespace esphome {
namespace hms {

/// Sortie float standard ESPHome (0.0 - 1.0) pilotant la limite de puissance
/// relative du HMS. Permet de contrôler l'onduleur depuis n'importe quel
/// composant produisant un output float (PID, template, light, etc.).
class HMSPowerLimitPercentOutput : public output::FloatOutput {
 public:
  void set_parent(HMSComponent *parent) { this->parent_ = parent; }

 protected:
  void write_state(float state) override;
  HMSComponent *parent_{nullptr};
};

}  // namespace hms
}  // namespace esphome
