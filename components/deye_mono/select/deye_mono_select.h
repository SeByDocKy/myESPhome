#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "../deye_mono.h"

#include <vector>

namespace esphome::deye_mono {

/** Bit field of a register: the n-th option is selected when (register & mask) equals the n-th value added with
 *  add_value(). A write changes only the bits of the mask. */
class DeyeMonoSelect : public select::Select,
                       public Component,
                       public DeyeMonoItem,
                       public Parented<DeyeMonoHub> {
 public:
  void dump_config() override;
  void set_mask(uint16_t mask) { this->mask_ = mask; }
  void add_value(uint16_t value) { this->values_.push_back(value); }
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  void control(size_t index) override;

  uint16_t mask_{0xFFFF};
  std::vector<uint16_t> values_;
};

}  // namespace esphome::deye_mono
