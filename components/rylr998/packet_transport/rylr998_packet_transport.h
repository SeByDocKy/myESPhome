#pragma once

#include "esphome/core/component.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "../rylr998.h"

namespace esphome {
namespace rylr998 {

static const char *const TAG_PT = "rylr998.packet_transport";

// NO inheritance from RYLR998Listener - eliminates the dual-vtable corruption
// that caused LoadProhibited crashes when PacketTransport::setup() corrupted
// the RYLR998Listener vtable pointer inside RYLR998Transport.
class RYLR998Transport : public packet_transport::PacketTransport {
 public:
  void set_parent(RYLR998Component *parent) { parent_static_ = parent; }

  void setup() override {
    // Store self in static BEFORE PacketTransport::setup() which may corrupt heap
    transport_static_ = this;
    PacketTransport::setup();
    // Register plain C callback - no vtable, no virtual dispatch, no heap
    if (parent_static_ != nullptr) {
      parent_static_->set_raw_packet_callback(RYLR998Transport::on_packet_static_);
    } else {
      ESP_LOGE(TAG_PT, "set_parent was never called!");
    }
  }

  void dump_config() override;

  // PacketTransport interface
  void send_packet(const std::vector<uint8_t> &buf) const override;
  size_t get_max_packet_size() override { return 240; }

 private:
  // Static plain C callback - called by RYLR998Component when packet received.
  // NO vtable lookup whatsoever. Reads transport_static_ from .bss section.
  static void on_packet_static_(const std::vector<uint8_t> &data, float rssi, float snr) {
    if (transport_static_ == nullptr) return;
    ESP_LOGD(TAG_PT, "RX packet %d bytes RSSI=%.0f SNR=%.0f", (int)data.size(), rssi, snr);
    transport_static_->process_(data);
  }

  // Both statics live in .bss - never on heap, never corrupted by heap allocations
  static RYLR998Component *parent_static_;
  static RYLR998Transport *transport_static_;
};

}  // namespace rylr998
}  // namespace esphome
