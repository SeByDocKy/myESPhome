#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "meshtastic.h"

namespace esphome {
namespace meshtastic {

template<typename... Ts> class SendTextMessageAction : public Action<Ts...>, public Parented<Meshtastic> {
 public:
  TEMPLATABLE_VALUE(std::string, text)
  TEMPLATABLE_VALUE(NodeNum, to)
  TEMPLATABLE_VALUE(std::string, channel)

  void play(const Ts &...x) override { this->parent_->send_text_message(this->to_.value(x...), this->channel_.value(x...), this->text_.value(x...)); }
};

template<typename... Ts> class SendPacketAction : public Action<Ts...>, public Parented<Meshtastic> {
 public:
  void set_data_template(std::vector<uint8_t> (*func)(Ts...)) {
    this->data_.func = func;
    this->len_ = -1;  // Sentinel value indicates template mode
  }

  void set_data_static(const uint8_t *data, size_t len) {
    this->data_.data = data;
    this->len_ = len;  // Length >= 0 indicates static mode
  }

  void play(const Ts &...x) override {
    std::vector<uint8_t> data;
    if (this->len_ >= 0) {
      // Static mode: copy from flash to vector
      data.assign(this->data_.data, this->data_.data + this->len_);
    } else {
      // Template mode: call function
      data = this->data_.func(x...);
    }
    this->parent_->transmit_packet(data, this->parent_->hop_limit_);
  }

 protected:
  ssize_t len_{-1};  // -1 = template mode, >=0 = static mode with length
  union Data {
    std::vector<uint8_t> (*func)(Ts...);  // Function pointer (stateless lambdas)
    const uint8_t *data;                  // Pointer to static data in flash
  } data_;
};

}  // namespace meshtastic
}  // namespace esphome
