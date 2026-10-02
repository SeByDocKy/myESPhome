#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "../marstek.h"

namespace esphome::marstek {

/// Writable numeric register (power limits, SOC targets, cut-offs, schedule start / end / power / days mask).
/// The value shown is `register x scale`; a write sends `value / scale` rounded to the register.
class MarstekNumber : public number::Number,
                      public Component,
                      public MarstekRegisterItem,
                      public Parented<MarstekHub> {
 public:
  void dump_config() override;
  void set_scale(float scale) { this->scale_ = scale != 0.0f ? scale : 1.0f; }
  void parse_and_publish(const std::vector<uint8_t> &data) override;

 protected:
  void control(float value) override;

  float scale_{1.0f};
};

}  // namespace esphome::marstek
