#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/apszb/apszb.h"

#include <cmath>

namespace esphome {
namespace apszb {

// Standard ESPHome float output (0.0 - 1.0) driving the power limit (throttling) of one inverter, so that any component
// producing a float (PID controller, template, light...) can regulate it. 1.0 = rated power of the inverter.
// Successive writes are coalesced by the hub: only the latest value of a burst is sent to the inverter.
class APSPowerLimitPercentOutput : public output::FloatOutput, public Parented<APSInverter> {
 protected:
  void write_state(float state) override {
    const float rated = float(this->parent_->rated_power());
    // the inverters refuse very low limits: never go below the minimum accepted by the `max_power` number (30 W)
    float watts = std::fmax(MIN_POWER_W, std::fmin(rated, std::roundf(state * rated)));
    if (watts == this->last_watts_)
      return;
    this->last_watts_ = watts;
    this->parent_->request_max_power(watts);
  }
  static constexpr float MIN_POWER_W = 30.0f;
  float last_watts_{-1.0f};
};

}  // namespace apszb
}  // namespace esphome
