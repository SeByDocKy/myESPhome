#include "rylr998_packet_transport.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rylr998 {

// Both statics in .bss - immune to heap corruption
RYLR998Component *RYLR998Transport::parent_static_ = nullptr;
RYLR998Transport *RYLR998Transport::transport_static_ = nullptr;

void RYLR998Transport::dump_config() {
  ESP_LOGCONFIG(TAG_PT, "RYLR998 Packet Transport");
}

void RYLR998Transport::send_packet(const std::vector<uint8_t> &buf) const {
  if (parent_static_ == nullptr) {
    ESP_LOGE(TAG_PT, "send_packet: parent is null!");
    return;
  }
  parent_static_->transmit_packet(buf);
}

}  // namespace rylr998
}  // namespace esphome
