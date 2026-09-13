#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../nrf24l01.h"

namespace esphome {
namespace nrf24l01 {

/// Publishes the radio duty cycle (% of time locked by an external_mode
/// component, e.g. hm:) over the window elapsed since the last publish
/// (governed by update_interval). Not meaningful if nrf24l01: is used in
/// generic mode (never locked) -- then it always publishes 0%.
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
