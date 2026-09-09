#include "packet_transport.h"

namespace esphome {
namespace nrf24l01 {

void NRF24L01Transport::setup() {
  PacketTransport::setup();
  this->parent_->add_on_packet_received_callback([this](std::vector<uint8_t> packet) { this->process_(packet); });
}

void NRF24L01Transport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->send_packet(buf); }

}  // namespace nrf24l01
}  // namespace esphome
