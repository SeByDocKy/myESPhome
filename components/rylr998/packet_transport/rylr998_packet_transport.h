#pragma once

#include "esphome.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "../rylr998.h"

namespace esphome {
namespace rylr998 {

class RYLR998PacketTransportComponent : public packet_transport::PacketTransport, public Parented<RYLR998Component>, public RYLR998Listener {
 public:
  
  void setup() override;

  // void set_parent(RYLR998Component *parent) { this->parent_ = parent; }

  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  
  // void dump_config() override;
  
  // void send_packet(const uint8_t *data, size_t len) override;
  
 protected:
  // RYLR998Component *parent_{nullptr};
  void send_packet(const std::vector<uint8_t> &buf) const override;
  bool should_send() override { return true; }
  size_t get_max_packet_size() override { return this->parent_->get_max_packet_size(); }
};

}  // namespace rylr998
}  // namespace esphome
