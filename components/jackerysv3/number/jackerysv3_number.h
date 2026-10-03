#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "../jackerysv3.h"

namespace esphome::jackerysv3 {

/// Writable setting of the battery (SOC charge / discharge limits, maximum grid output power).
/// The value shown comes from the battery's reports; the limits reported by the battery (minSocChg,
/// maxSocChg ...) narrow the slider at run time.
class JackerySV3Number : public number::Number, public Component, public Parented<JackerySV3Hub> {
 public:
  void dump_config() override;
  void set_kind(NumberKind kind) { this->kind_ = kind; }

 protected:
  void control(float value) override;

  NumberKind kind_{NumberKind::SOC_CHARGE_LIMIT};
};

}  // namespace esphome::jackerysv3
