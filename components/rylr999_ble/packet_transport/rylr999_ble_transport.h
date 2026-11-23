#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "esphome/components/rylr999_ble/rylr999_ble.h"
#include <vector>

namespace esphome::rylr999_ble {

// Bridges packet_transport::PacketTransport to the rylr999_ble component: outgoing frames go
// through send_payload() (queued, hex-encoded, sent over BLE with AT+SEND), incoming frames arrive
// through on_packet() when the parent has decoded a "+RCV=..." line. Same principle as
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
  // No point building frames before the radio is configured (AT+ADDRESS ... AT+CRFOP):
  // send_payload() would refuse them anyway.
  bool should_send() override { return this->parent_->is_lora_ready(); }
  size_t get_max_packet_size() override { return this->parent_->get_max_packet_size(); }
};

}  // namespace esphome::rylr999_ble
