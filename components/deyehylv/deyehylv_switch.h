#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SWITCH

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "deyehylv.h"

namespace esphome::deyehylv {

/** One bit field of a register, on when (register & mask) is not zero. A write changes only the bits of the mask:
 *  several switches and selects share some registers (28, 247, 248...), and the stock modbus switch would
 *  overwrite the others with zeros. */
class DeyeHyLvSwitch : public switch_::Switch,
                       public Component,
                       public DeyeHyLvItem,
                       public Parented<DeyeHyLvHub> {
 public:
  void dump_config() override;
  void set_mask(uint16_t mask) { this->mask_ = mask; }
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  void write_state(bool state) override;

  uint16_t mask_{0xFFFF};
};

}  // namespace esphome::deyehylv

#endif  // USE_SWITCH
