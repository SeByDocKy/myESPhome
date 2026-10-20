#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"

#include "esphome/components/faikin/faikin.h"

namespace esphome {
namespace faikin {

/// Climate entity: power / mode / target temperature / fan / swing, with the room temperature as
/// current temperature. The fan has the standard AUTO and QUIET modes plus custom modes "1" to "5".
class FaikinClimate : public climate::Climate, public Component, public Parented<FaikinComponent> {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  /// Copies the hub state into the climate entity and publishes it.
  void update_from_hub_();
};

}  // namespace faikin
}  // namespace esphome
