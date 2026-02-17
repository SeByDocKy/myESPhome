#include "rylr998_packet_transport.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rylr998 {

// Static member definition - lives in .bss, not in object heap memory
RYLR998Component *RYLR998Transport::rylr998_parent_static_ = nullptr;

void RYLR998Transport::dump_config() {
  ESP_LOGCONFIG(TAG_PT, "RYLR998 Packet Transport");
}

void RYLR998Transport::send_packet(const std::vector<uint8_t> &buf) const {
  if (rylr998_parent_static_ == nullptr) {
    ESP_LOGE(TAG_PT, "send_packet: parent is null!");
    return;
  }
  rylr998_parent_static_->transmit_packet(buf);
}

void RYLR998Transport::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) {
  this->process_(packet);
}

}  // namespace rylr998
}  // namespace esphome
