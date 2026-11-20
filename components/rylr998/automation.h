#pragma once

#include <functional>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/log.h"
#include "rylr998.h"

namespace esphome::rylr998 {

// Trigger fired when a LoRa packet is received (one instance per `on_packet:` entry).
class RYLR998PacketTrigger : public Trigger<std::vector<uint8_t>, float, float> {};

// rylr998.send_packet action. The packet is queued (see RYLR998Component::transmit_packet), so the
// action never blocks the main loop.
template<typename... Ts> class RYLR998SendPacketAction : public Action<Ts...> {
 public:
  explicit RYLR998SendPacketAction(RYLR998Component *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(uint16_t, destination)

  void set_data_template(std::function<std::vector<uint8_t>(const Ts &...)> func) {
    this->data_func_ = std::move(func);
    this->static_ = false;
  }

  void set_data_static(const std::vector<uint8_t> &data) {
    this->data_static_ = data;
    this->static_ = true;
  }

  // Signature must match Action<Ts...>::play(const Ts &...)
  void play(const Ts &...x) override {
    const uint16_t dest = this->destination_.value(x...);
    bool ok;
    if (this->static_) {
      ok = this->parent_->transmit_packet(dest, this->data_static_);  // no copy
    } else {
      ok = this->parent_->transmit_packet(dest, this->data_func_(x...));
    }
    if (!ok) {
      ESP_LOGW("rylr998", "rylr998.send_packet: packet refused");
    }
  }

 protected:
  RYLR998Component *parent_;
  bool static_{true};
  std::vector<uint8_t> data_static_{};
  std::function<std::vector<uint8_t>(const Ts &...)> data_func_{};
};

}  // namespace esphome::rylr998
