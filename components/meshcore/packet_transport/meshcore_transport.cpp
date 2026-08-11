#include "esphome/core/log.h"
#include "meshcore_transport.h"

namespace esphome {
namespace meshcore {

static const char *const TAG = "meshcore.packet_transport";

void MeshCoreTransport::setup() {
  PacketTransport::setup();

  if (this->meshcore_ == nullptr) {
    ESP_LOGE(TAG, "Aucun composant meshcore associe (meshcore_id manquant)");
    this->mark_failed();
    return;
  }

  // On s'abonne aux paquets GRP_DATA du composant meshcore parent, et on ne
  // traite que ceux du canal qui nous concerne (un meme "meshcore:" peut
  // porter plusieurs canaux / plusieurs transports).
  this->meshcore_->add_data_listener([this](std::string channel, std::vector<uint8_t> data) {
    if (channel != this->channel_)
      return;
    this->process_(std::span<const uint8_t>(data.data(), data.size()));
  });
}

void MeshCoreTransport::dump_config() {
  PacketTransport::dump_config();
  ESP_LOGCONFIG(TAG, "  Canal MeshCore: %s", this->channel_.c_str());
}

void MeshCoreTransport::send_packet(const std::vector<uint8_t> &buf) const {
  if (this->meshcore_ == nullptr)
    return;
  this->meshcore_->send_group_data(this->channel_, buf);
}

size_t MeshCoreTransport::get_max_packet_size() { return MeshCore::max_group_data_size(); }

}  // namespace meshcore
}  // namespace esphome
