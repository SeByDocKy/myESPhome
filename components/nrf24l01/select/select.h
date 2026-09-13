#pragma once

#include "esphome/components/select/select.h"
#include "../nrf24l01.h"

namespace esphome {
namespace nrf24l01 {

/// Réglage du niveau PA (min/low/high/max) à la volée, sans reset -- même
/// principe que cmt2300a::CMT2300APALevelNumber, adapté ici en select puisque
/// le nRF24L01 n'a que 4 niveaux discrets (pas une plage continue en dBm).
class NRF24PALevelSelect : public select::Select {
 public:
  void set_parent(NRF24Component *parent) { this->parent_ = parent; }

 protected:
  void control(const std::string &value) override;
  NRF24Component *parent_{nullptr};
};

}  // namespace nrf24l01
}  // namespace esphome
