#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../ecoflow_stream.h"

namespace esphome::ecoflow_stream {

class EcoflowStreamSwitch : public switch_::Switch,
                            public Component,
                            public EcoflowStreamRegisterItem,
                            public Parented<EcoflowStreamHub> {
 public:
  void setup() override { this->writer_.init(this->parent_->controller()); }
  void dump_config() override;
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  void write_state(bool state) override;

  EcoflowStreamWriter writer_;
};

/// Not a register: stops / restarts the heartbeat of the hub
class EcoflowStreamHeartbeatSwitch : public switch_::Switch, public Component, public Parented<EcoflowStreamHub> {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::ecoflow_stream
