#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "meshcore.h"

namespace esphome {
namespace meshcore {

template<typename... Ts> class SendGroupTextAction : public Action<Ts...>, public Parented<MeshCore> {
 public:
  TEMPLATABLE_VALUE(std::string, channel)
  TEMPLATABLE_VALUE(std::string, text)

  void play(const Ts &...x) override {
    this->parent_->send_group_text(this->channel_.value(x...), this->text_.value(x...));
  }
};

}  // namespace meshcore
}  // namespace esphome
