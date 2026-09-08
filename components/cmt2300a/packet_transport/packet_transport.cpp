#include "esphome/core/log.h"
#include "packet_transport.h"

namespace esphome {
namespace cmt2300a {

static const char *const TAG = "cmt2300a.packet_transport";

void CMT2300ATransport::setup() {
  if (this->parent_->get_external_mode()) {
    // Cette instance cmt2300a: est pilotée par un hms: (bancs de registres et
    // cadence Tx/Rx spécifiques Hoymiles) -- incompatible avec le mode générique
    // qu'utilise packet_transport. Erreur explicite plutôt qu'un comportement
    // silencieusement cassé (les deux se marcheraient dessus sur la même puce).
    ESP_LOGE(TAG, "Ce cmt2300a: est déjà utilisé en mode externe par un composant hms: -- "
                  "packet_transport ne peut pas partager la même puce dans ce mode. "
                  "Utilise un cmt2300a: dédié (autres broches) pour packet_transport.");
    this->mark_failed();
    return;
  }

  PacketTransport::setup();
  this->parent_->add_on_packet_received_callback([this](std::vector<uint8_t> packet) { this->process_(packet); });
}

void CMT2300ATransport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->send_packet(buf); }

}  // namespace cmt2300a
}  // namespace esphome
