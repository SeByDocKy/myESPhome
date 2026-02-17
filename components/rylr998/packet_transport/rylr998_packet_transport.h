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
  void set_parent(RYLR998Component *parent) { this->rylr998_parent_ = parent; }

  void setup() override {
    PacketTransport::setup();
    this->rylr998_parent_->register_listener(this);  // register here, after PacketTransport::setup()
  }

  void dump_config() override;

  void update() override { this->send_data_(false); }

  // PacketTransport interface
  void send_packet(const std::vector<uint8_t> &buf) const override;
  size_t get_max_packet_size() override { return 240; }

  // RYLR998Listener interface
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;

 protected:
  RYLR998Component *rylr998_parent_{nullptr};
};

}  // namespace rylr998
}  // namespace esphome
