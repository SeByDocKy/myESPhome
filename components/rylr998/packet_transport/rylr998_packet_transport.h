#pragma once

#include "esphome/core/component.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "../rylr998.h"

namespace esphome {
namespace rylr998 {

static const char *const TAG_PT = "rylr998.packet_transport";

// Packet Transport - exactly like SX127xTransport
class RYLR998Transport : public packet_transport::PacketTransport, 
                         public PollingComponent,
                         public RYLR998Listener {
 public:
  void set_parent(RYLR998Component *parent) { 
    this->parent_ = parent;
    parent->register_listener(this);
  }
  
  void setup() override {}
  void dump_config() override;
  
  // PacketTransport interface
  void send_packet(const std::vector<uint8_t> &buf) const override;
  
  // RYLR998Listener interface
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;
  
 protected:
  RYLR998Component *parent_{nullptr};
};

}  // namespace rylr998
}  // namespace esphome
