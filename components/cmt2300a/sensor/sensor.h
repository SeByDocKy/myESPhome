#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../cmt2300a.h"

namespace esphome {
namespace cmt2300a {

/// Publishes the radio duty cycle (% of time locked by an external_mode
/// component, e.g. hms:) over the window elapsed since the last publish
/// (governed by update_interval). Not meaningful if cmt2300a: is used in
/// generic mode (never locked) -- then it always publishes 0%.
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
