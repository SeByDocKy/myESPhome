#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "../cmt2300a.h"

namespace esphome {
namespace cmt2300a {

// Implémente le "medium" packet_transport pour un CMT2300A en mode générique
// (pas hms:). Basé sur le même patron que sx126x/sx127x/udp côté ESPHome
// (packet_transport::PacketTransport + Parented<T>).
class CMT2300ATransport : public packet_transport::PacketTransport, public Parented<CMT2300AComponent> {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  size_t get_max_packet_size() override { return 32; }  // taille FIFO du CMT2300A en mode générique
};

}  // namespace cmt2300a
}  // namespace esphome
