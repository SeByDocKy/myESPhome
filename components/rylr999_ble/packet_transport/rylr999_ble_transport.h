#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "esphome/components/rylr999_ble/rylr999_ble.h"
#include <vector>

namespace esphome::rylr999_ble {

// Relie packet_transport::PacketTransport au composant rylr999_ble : les
// paquets sortants passent par send_payload() (hex-encodés puis envoyés en
// BLE via AT+SEND), les paquets entrants arrivent via on_packet() lorsque le
// composant parent a décodé une trame "+RCV=...". Même principe que
// sx126x::SX126xTransport.
class RYLR999BLETransport final : public packet_transport::PacketTransport,
                                   public Parented<RYLR999BLEComponent>,
                                   public RYLR999BLEListener {
 public:
  void setup() override;
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  // Pas d'intérêt à tenter un envoi tant que la configuration LoRa BLE n'est
  // pas validée (AT+ADDRESS...AT+CRFOP) : le paquet serait de toute façon
  // rejeté par send_payload().
  bool should_send() override { return this->parent_->is_lora_ready(); }
  size_t get_max_packet_size() override { return this->parent_->get_max_packet_size(); }
};

}  // namespace esphome::rylr999_ble
