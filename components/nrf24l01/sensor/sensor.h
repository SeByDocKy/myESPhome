#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../nrf24l01.h"

namespace esphome {
namespace nrf24l01 {

/// Publie le taux d'occupation radio (% de temps verrouillé par un composant
/// external_mode, ex. hm:) sur la fenêtre écoulée depuis la dernière publication
/// (gouvernée par update_interval). Non pertinent si nrf24l01: est utilisé en
/// mode générique (jamais verrouillé) -- publie alors toujours 0%.
class NRF24DutyCycleSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(NRF24Component *parent) { this->parent_ = parent; }
  void update() override { this->publish_state(this->parent_->get_duty_cycle_percent_and_reset()); }
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  NRF24Component *parent_{nullptr};
};

}  // namespace nrf24l01
}  // namespace esphome
