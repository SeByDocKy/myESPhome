#pragma once

#include "esphome/components/button/button.h"
#include "../hms.h"

namespace esphome {
namespace hms {

/// Bouton générique : à l'appui, force la limite de puissance relative du HMS à
/// une valeur cible fixe (utilisé pour reset_to_output_min / reset_to_output_max).
class HMSResetPercentButton : public button::Button {
 public:
  void set_parent(HMSComponent *parent) { this->parent_ = parent; }
  void set_target_percent(float percent) { this->target_percent_ = percent; }

 protected:
  void press_action() override;
  HMSComponent *parent_{nullptr};
  float target_percent_{100.0f};
};

}  // namespace hms
}  // namespace esphome
