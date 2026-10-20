#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/faikin/faikin.h"

namespace esphome {
namespace faikin {

/// One switch class for all the boolean features, selected by `flag`. The hub publishes the state it
/// reads from the unit; write_state() only forwards the request.
class FaikinSwitch : public switch_::Switch, public Parented<FaikinComponent> {
 public:
  void set_flag(Flag flag) { this->flag_ = flag; }

 protected:
  void write_state(bool state) override;

  Flag flag_{Flag::ECONO};
};

}  // namespace faikin
}  // namespace esphome
