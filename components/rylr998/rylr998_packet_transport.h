#pragma once

#include "esphome/core/component.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "rylr998.h"

namespace esphome {
namespace rylr998 {

static const char *const TAG_PT = "rylr998.packet_transport";

// Packet Transport Component
class RYLR998PacketTransportComponent : public packet_transport::PacketTransportComponent {
 public:
  void set_parent(RYLR998Component *parent) { this->parent_ = parent; }
  
  void setup() override;
  void dump_config() override;
  
  bool can_proceed() override { return this->parent_ != nullptr; }
  
 protected:
  void send_packet_(const packet_transport::PacketTransportHeader &header, 
                   const uint8_t *data, uint8_t data_len) override;
  
  RYLR998Component *parent_{nullptr};
};

}  // namespace rylr998
}  // namespace esphome
