#pragma once

#include "esphome/core/automation.h"
#include "rylr998.h"

namespace esphome {
namespace rylr998 {

// Automation trigger - uses the component's trigger
class RYLR998PacketTrigger : public Trigger<std::vector<uint8_t>, float, float> {
 public:
  explicit RYLR998PacketTrigger(RYLR998Component *parent) {
    parent->get_packet_trigger()->add_on_trigger_callback([this](std::vector<uint8_t> data, float rssi, float snr) {
      this->trigger(data, rssi, snr);
    });
  }
};

// Automation action for sending packets
template<typename... Ts>
class RYLR998SendPacketAction : public Action<Ts...> {
 public:
  explicit RYLR998SendPacketAction(RYLR998Component *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(uint16_t, destination)
  
  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) {
    this->data_func_ = func;
    this->static_ = false;
  }
  
  void set_data_static(const std::vector<uint8_t> &data) {
    this->data_static_ = data;
    this->static_ = true;
  }

  void play(Ts... x) override {
    auto dest = this->destination_.value(x...);
    auto data = this->static_ ? this->data_static_ : this->data_func_(x...);
    this->parent_->transmit_packet(dest, data);
  }

 protected:
  RYLR998Component *parent_;
  bool static_{true};
  std::vector<uint8_t> data_static_{};
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
};

}  // namespace rylr998
}  // namespace esphome
