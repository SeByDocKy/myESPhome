#pragma once

#include "esphome/components/button/button.h"
#include "../hm.h"

namespace esphome {
namespace hm {

/// Bouton générique : à l'appui, force la limite de puissance relative du HM à
/// une valeur cible fixe (utilisé pour reset_to_output_min / reset_to_output_max),
/// en écriture PERSISTANTE (EEPROM onduleur).
class HMResetPercentButton : public button::Button {
 public:
  void set_parent(HMComponent *parent) { this->parent_ = parent; }
  void set_target_percent(float percent) { this->target_percent_ = percent; }

 protected:
  void press_action() override;
  HMComponent *parent_{nullptr};
  float target_percent_{100.0f};
};

/// A l'appui, reset matériel de la puce nRF24L01 (sans reboot de l'ESP32) suivi
/// d'une reconfiguration Hoymiles NRF complète.
class HMResetHmButton : public button::Button {
 public:
  void set_parent(HMComponent *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  HMComponent *parent_{nullptr};
};

}  // namespace hm
}  // namespace esphome
