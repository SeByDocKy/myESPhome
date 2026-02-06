#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "rylr998_packet_transport.h"


namespace esphome {
namespace rylr998 {

static const char *const TAG = "rylr998_transport";

void RYLR998Transport::setup() {
  PacketTransport::setup();
  this->parent_->register_listener(this);
}

void RYLR998Transport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->transmit_packet(buf); }

void RYLR998Transport::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) { this->process_(packet); }

// void RYLR998PTransport::setup() {
//   // Register callback to receive packets
//   this->parent_->add_on_packet_callback(
//       [this](uint16_t address, std::vector<uint8_t> data, int rssi, int snr) {
//         // Parse the packet header and data
//         if (data.size() < sizeof(packet_transport::PacketTransportHeader)) {
//           ESP_LOGW(TAG_PT, "Packet too small for header");
//           return;
//         }
        
//         packet_transport::PacketTransportHeader header;
//         memcpy(&header, data.data(), sizeof(header));
        
//         // Extract payload
//         const uint8_t *payload = data.data() + sizeof(header);
//         uint8_t payload_len = data.size() - sizeof(header);
        
//         // Process the packet through the transport layer
//         this->on_packet_(header, payload, payload_len);
//       });
// }

// void RYLR998PTransport::dump_config() {
//   ESP_LOGCONFIG(TAG_PT, "RYLR998 Packet Transport:");
//   this->dump_config_();
// }

// void RYLR998PTransport::send_packet_(
//     const packet_transport::PacketTransportHeader &header,
//     const uint8_t *data, uint8_t data_len) {
  
//   // Build packet with header + data
//   std::vector<uint8_t> packet;
//   packet.resize(sizeof(header) + data_len);
  
//   // Copy header
//   memcpy(packet.data(), &header, sizeof(header));
  
//   // Copy data
//   if (data_len > 0) {
//     memcpy(packet.data() + sizeof(header), data, data_len);
//   }
  
//   // Send to destination address (use broadcast address 0 for now)
//   uint16_t destination = 0;  // Broadcast
  
//   if (!this->parent_->send_data(destination, packet)) {
//     ESP_LOGW(TAG_PT, "Failed to send packet transport packet");
//   }
// }

}  // namespace rylr998
}  // namespace esphome
