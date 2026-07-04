#pragma once

#include "esphome/core/component.h"
#include "../meshtastic.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include <vector>

namespace esphome {
namespace meshtastic {

class MeshtasticTransport : public packet_transport::PacketTransport, public Parented<Meshtastic> {
 private:
  uint8_t hop_limit_ = 0;
 public:
  void setup() override;
  void on_packet(uint32_t from, uint32_t to, int32_t port, std::vector<uint8_t> &data);
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void set_hop_limit(uint8_t hop_limit) { this->hop_limit_ = hop_limit; };

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  bool should_send() override { return true; }
  size_t get_max_packet_size() override { return this->parent_->get_max_packet_size(); }
};

}  // namespace meshtastic
}  // namespace esphome
