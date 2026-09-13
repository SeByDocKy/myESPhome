#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "../cmt2300a.h"

namespace esphome {
namespace cmt2300a {

// Implements the packet_transport "medium" for a CMT2300A in generic mode
// (not hms:). Based on the same pattern as ESPHome's sx126x/sx127x/udp
// (packet_transport::PacketTransport + Parented<T>).
class CMT2300ATransport : public packet_transport::PacketTransport, public Parented<CMT2300AComponent> {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  size_t get_max_packet_size() override { return 32; }  // CMT2300A FIFO size in generic mode
};

}  // namespace cmt2300a
}  // namespace esphome
