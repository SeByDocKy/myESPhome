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
  // Store parent in a static variable - completely outside the object's heap memory.
  // PacketTransport::setup() overwrites any instance member at offset > ~290 bytes,
  // but static members live in .bss and are never touched by base class setup.
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

  void update() override { this->send_data_(false); }

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
