#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SELECT

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "esphome/components/deyelvhy/deyelvhy.h"

#include <vector>

namespace esphome::deyelvhy {

/** Bit field of a register: the n-th option is selected when (register & mask) equals the n-th value added with
 *  add_value(). A write changes only the bits of the mask. */
class DeyeLvHySelect : public select::Select,
                       public Component,
                       public DeyeLvHyItem,
                       public Parented<DeyeLvHyHub> {
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

}  // namespace esphome::deyelvhy

#endif  // USE_SELECT
