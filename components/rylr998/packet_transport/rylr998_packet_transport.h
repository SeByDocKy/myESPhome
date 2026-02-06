#pragma once

#include "esphome.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "../rylr998.h"

namespace esphome {
namespace rylr998 {

class RYLR998PacketTransportComponent : public Component, public packet_transport::PacketTransport {
 public:
  void set_parent(RYLR998Component *parent) { this->parent_ = parent; }
  
  void setup() override;
  void dump_config() override;
  
  void send_packet(const uint8_t *data, size_t len) override;
  
 protected:
  RYLR998Component *parent_{nullptr};
};

}  // namespace rylr998
}  // namespace esphome
