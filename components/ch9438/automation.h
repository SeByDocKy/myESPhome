/// @file automation.h
/// @brief WCH CH9438 - automation actions

#pragma once
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/ch9438/ch9438.h"

namespace esphome::ch9438 {

/// @brief Action `ch9438.change_baud_rate`: changes the baud rate of one UART at run time
template<typename... Ts> class ChangeBaudRateAction final : public Action<Ts...>, public Parented<CH9438Channel> {
 public:
  TEMPLATABLE_VALUE(uint32_t, baud_rate)

  void play(const Ts &...x) override { this->parent_->change_baud_rate(this->baud_rate_.value(x...)); }
};

}  // namespace esphome::ch9438
