#include "esphome/core/log.h"
#include "packet_transport.h"

namespace esphome {
namespace cmt2300a {

static const char *const TAG = "cmt2300a.packet_transport";

void CMT2300ATransport::setup() {
  if (this->parent_->get_external_mode()) {
    // This cmt2300a: instance is driven by an hms: (Hoymiles-specific register
    // banks and Tx/Rx cadence) -- incompatible with the generic mode that
    // packet_transport uses. Explicit error rather than silently broken
    // behavior (both would step on each other on the same chip).
    ESP_LOGE(TAG, "This cmt2300a: is already used in external mode by an hms: component -- "
                  "packet_transport cannot share the same chip in this mode. "
                  "Use a dedicated cmt2300a: (other pins) for packet_transport.");
    this->mark_failed();
    return;
  }

  PacketTransport::setup();
  this->parent_->add_on_packet_received_callback([this](std::vector<uint8_t> packet) { this->process_(packet); });
}

void CMT2300ATransport::send_packet(const std::vector<uint8_t> &buf) const { this->parent_->send_packet(buf); }

}  // namespace cmt2300a
}  // namespace esphome
