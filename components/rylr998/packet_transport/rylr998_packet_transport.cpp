#include "rylr998_packet_transport.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rylr998 {

void RYLR998Transport::dump_config() {
  ESP_LOGCONFIG(TAG_PT, "RYLR998 Packet Transport");
}

void RYLR998Transport::send_packet(const std::vector<uint8_t> &buf) const {
  // Send packet using the parent RYLR998 component
  this->parent_->transmit_packet(buf);
}

void RYLR998Transport::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) {
  // Process the packet through the transport layer
  this->process_(packet);
}

}  // namespace rylr998
}  // namespace esphome
