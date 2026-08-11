#pragma once

#include "esphome/core/component.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "../meshcore.h"

#include <vector>
#include <string>

namespace esphome {
namespace meshcore {

class MeshCoreTransport final : public packet_transport::PacketTransport {
 public:
  void set_meshcore(MeshCore *meshcore) { this->meshcore_ = meshcore; }
  void set_channel(const std::string &channel) { this->channel_ = channel; }

  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  size_t get_max_packet_size() override;

  MeshCore *meshcore_{nullptr};
  std::string channel_;
};

}  // namespace meshcore
}  // namespace esphome
