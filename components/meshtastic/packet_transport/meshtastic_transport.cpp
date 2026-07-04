#include "meshtastic_transport.h"

namespace esphome {
namespace meshtastic {

static const char *const TAG = "meshtastic_transport";

void MeshtasticTransport::setup() {
  PacketTransport::setup();
  this->parent_->add_listener([this](uint32_t from, uint32_t to, int32_t port, std::vector<uint8_t> &data) {
    if(port == meshtastic_PortNum_DETECTION_SENSOR_APP) {
      ESP_LOGD(TAG, "MeshtasticTransport::on_packet port=%d, data %s", port, format_hex_pretty(data, ',', true).c_str());
      this->process_(data);
    }
  });
}

void MeshtasticTransport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->transmit_packet(buf, hop_limit_); }

}  // namespace meshtastic
}  // namespace esphome
