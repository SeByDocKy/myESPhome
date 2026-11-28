#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../ecoflow_stream.h"

namespace esphome::ecoflow_stream {

class EcoflowStreamSensor : public sensor::Sensor, public Component, public EcoflowStreamRegisterItem {
 public:
  void dump_config() override;
  void parse_and_publish(std::span<const uint8_t> data) override;
};

}  // namespace esphome::ecoflow_stream
