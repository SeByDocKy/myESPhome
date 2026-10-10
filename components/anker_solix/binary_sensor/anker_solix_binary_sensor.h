#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../anker_solix.h"

namespace esphome::anker_solix {

/// Register that is 0 / 1 (backup SOC function enabled). The connection sensor needs no class of its own: the hub
/// drives a plain BinarySensor (see AnkerSolixHub::set_connection_sensor()).
class AnkerSolixBinarySensor : public binary_sensor::BinarySensor, public Component, public AnkerSolixRegisterItem {
 public:
  void dump_config() override;
  void parse_and_publish(std::span<const uint8_t> data) override;
};

}  // namespace esphome::anker_solix
