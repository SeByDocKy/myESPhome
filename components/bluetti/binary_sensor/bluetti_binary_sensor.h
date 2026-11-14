#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../bluetti.h"

namespace esphome::bluetti {

/// Register that is 0 / 1 (AC output, grid charging, grid feed-in, system on/off). The connection sensor needs no
/// class of its own: the hub drives a plain BinarySensor (see BluettiHub::set_connection_sensor()).
class BluettiBinarySensor : public binary_sensor::BinarySensor, public Component, public BluettiRegisterItem {
 public:
  void dump_config() override;
  void parse_and_publish(std::span<const uint8_t> data) override;
};

}  // namespace esphome::bluetti
