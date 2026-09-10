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

/// A l'appui, reset matériel de la puce CMT2300A (sans reboot de l'ESP32) suivi
/// d'une reconfiguration Hoymiles complète. Utilisé en secours si l'onduleur ne
/// se laisse jamais joindre après un boot de l'ESP32 -- voir la doc pour une
/// automatisation qui appuie dessus périodiquement tant que reachable est off.
class HMSResetHmsButton : public button::Button {
 public:
  void set_parent(HMSComponent *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  HMSComponent *parent_{nullptr};
};

}  // namespace hms
}  // namespace esphome
