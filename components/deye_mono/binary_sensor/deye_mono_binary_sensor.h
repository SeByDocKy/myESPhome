#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../deye_mono.h"

namespace esphome::deye_mono {

/// State of a bit field of a register: on when (register & mask) is not zero. Read only: it shows what the inverter
/// reports, even right after a write (it is the feedback of the switches and selects sharing the register).
class DeyeMonoBinarySensor : public binary_sensor::BinarySensor, public Component, public DeyeMonoItem {
 public:
  void dump_config() override;
  void set_mask(uint16_t mask) { this->mask_ = mask; }
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  uint16_t mask_{0xFFFF};
};

}  // namespace esphome::deye_mono
