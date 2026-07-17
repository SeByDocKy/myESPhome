#include "rylr999_ble_transport.h"
#include "esphome/core/log.h"

namespace esphome::rylr999_ble {

static const char *const TAG = "rylr999_ble.packet_transport";

void RYLR999BLETransport::setup() {
  PacketTransport::setup();
  this->parent_->register_listener(this);
}

void RYLR999BLETransport::send_packet(const std::vector<uint8_t> &buf) const {
  if (!this->parent_->send_payload(buf)) {
    ESP_LOGD(TAG, "Envoi du paquet packet_transport (%u octets) différé/échoué", (unsigned) buf.size());
  }
}

void RYLR999BLETransport::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) {
  this->process_(packet);
}

}  // namespace esphome::rylr999_ble
