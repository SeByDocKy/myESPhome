#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "../nrf24l01.h"

namespace esphome {
namespace nrf24l01 {

// Même patron que cmt2300a::CMT2300ATransport / les sx126x, sx127x, udp
// officiels : packet_transport::PacketTransport + Parented<T>.
class NRF24L01Transport : public packet_transport::PacketTransport, public Parented<NRF24Component> {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  size_t get_max_packet_size() override { return 32; }
};

}  // namespace nrf24l01
}  // namespace esphome
