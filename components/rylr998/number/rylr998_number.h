#pragma once

#include "esphome/components/number/number.h"

namespace esphome {
namespace rylr998 {

// Forward declaration du composant parent
class RYLR998Component;

/**
 * RYLR998TxPowerNumber
 *
 * Number ESPHome permettant de contrôler la puissance d'émission (tx_power)
 * du module LoRa RYLR998 à chaud, sans redémarrage.
 *
 * Plage : 0 dBm à 22 dBm, pas de 1 dBm (commande AT+CRFOP=<value>).
 *
 * La méthode control() délègue à RYLR998Component::apply_tx_power() qui
 * envoie la commande AT et met à jour le membre tx_power_ du composant.
 */
class RYLR998TxPowerNumber : public number::Number {
 public:
  void set_parent(RYLR998Component *parent) { this->parent_ = parent; }

 protected:
  /**
   * Appelé par ESPHome lorsque l'utilisateur modifie la valeur dans l'UI.
   * Envoie AT+CRFOP=<value> via le composant parent et publie la nouvelle état.
   */
  void control(float value) override;

  RYLR998Component *parent_{nullptr};
};

}  // namespace rylr998
}  // namespace esphome
