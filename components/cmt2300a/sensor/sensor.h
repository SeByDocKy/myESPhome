#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../cmt2300a.h"

namespace esphome {
namespace cmt2300a {

/// Publie le taux d'occupation radio (% de temps verrouillé par un composant
/// external_mode, ex. hms:) sur la fenêtre écoulée depuis la dernière publication
/// (gouvernée par update_interval). Non pertinent si cmt2300a: est utilisé en
/// mode générique (jamais verrouillé) -- publie alors toujours 0%.
class CMT2300ADutyCycleSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(CMT2300AComponent *parent) { this->parent_ = parent; }
  void update() override { this->publish_state(this->parent_->get_duty_cycle_percent_and_reset()); }
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  CMT2300AComponent *parent_{nullptr};
};

}  // namespace cmt2300a
}  // namespace esphome
