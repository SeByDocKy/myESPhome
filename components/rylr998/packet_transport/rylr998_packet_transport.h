#pragma once

#include "esphome/core/component.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "../rylr998.h"

namespace esphome {
namespace rylr998 {

static const char *const TAG_PT = "rylr998.packet_transport";

class RYLR998Transport : public packet_transport::PacketTransport,
                         public RYLR998Listener {
 public:
  // set_parent stores the pointer ONLY - no register_listener here (SX127x pattern)
  void set_parent(RYLR998Component *parent) { rylr998_parent_static_ = parent; }

  void setup() override {
    PacketTransport::setup();
    // register_listener after PacketTransport::setup() - same pattern as SX127x
    if (rylr998_parent_static_ != nullptr) {
      rylr998_parent_static_->register_listener(this);
    } else {
      ESP_LOGE(TAG_PT, "set_parent was never called!");
    }
  }

  void dump_config() override;

  // DO NOT override update() - PacketTransport::update() already calls send_data_()
  // Overriding it causes double TX per cycle

  // PacketTransport interface
  void send_packet(const std::vector<uint8_t> &buf) const override;
  size_t get_max_packet_size() override { return 240; }

  // RYLR998Listener interface
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;

 private:
  // Static: lives in .bss, never on the heap, immune to PacketTransport::setup()
  static RYLR998Component *rylr998_parent_static_;
};

}  // namespace rylr998
}  // namespace esphome
