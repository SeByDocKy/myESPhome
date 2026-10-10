#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "../deye_mono.h"

namespace esphome::deye_mono {

/// Setting held in a whole register: value = raw * scale (e.g. 0.01 for a voltage in 10 mV steps).
class DeyeMonoNumber : public number::Number,
                       public Component,
                       public DeyeMonoItem,
                       public Parented<DeyeMonoHub> {
 public:
  void dump_config() override;
  void set_scale(float scale) { this->scale_ = scale; }
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  void control(float value) override;

  float scale_{1.0f};
};

}  // namespace esphome::deye_mono
