#pragma once

#include "esphome/components/number/number.h"
#include "../cmt2300a.h"

namespace esphome {
namespace cmt2300a {

/// Runtime transmit power adjustment (-10 to +20 dBm), no chip reset --
/// set_pa_level() only writes 2-3 registers directly, safe to call at any
/// time, including while an hms: is sharing the same radio.
class CMT2300APALevelNumber : public number::Number {
 public:
  void set_parent(CMT2300AComponent *parent) { this->parent_ = parent; }

 protected:
  void control(float value) override;
  CMT2300AComponent *parent_{nullptr};
};

}  // namespace cmt2300a
}  // namespace esphome
