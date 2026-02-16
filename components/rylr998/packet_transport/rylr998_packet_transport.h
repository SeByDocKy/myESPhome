#pragma once

#include "esphome/core/component.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "../rylr998.h"

namespace esphome {
namespace rylr998 {

static const char *const TAG_PT = "rylr998.packet_transport";

// Packet Transport - exactly like SX127xTransport
// Note: PacketTransport already inherits from PollingComponent
class RYLR998Transport : public packet_transport::PacketTransport, 
                         public RYLR998Listener {
 public:
  void set_parent(RYLR998Component *parent) { 
    this->parent_ = parent;
    parent->register_listener(this);
  }
  
  void setup() override {}
  void dump_config() override;
  
  // PollingComponent: appelé à chaque update_interval pour envoyer les données
  void update() override { this->send_data_(); }
  
  // PacketTransport interface
  void send_packet(const std::vector<uint8_t> &buf) const override;
  size_t get_max_packet_size() override { return 240; }  // RYLR998 max packet size
  
  // RYLR998Listener interface
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;
  
 protected:
  RYLR998Component *parent_{nullptr};
};

}  // namespace rylr998
}  // namespace esphome
