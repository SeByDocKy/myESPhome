#pragma once

#include "esphome/core/defines.h"

#ifdef USE_NUMBER

#include "esphome/components/number/number.h"
#include "esphome/core/helpers.h"
#include "../smartsolar.h"

namespace esphome {
namespace smartsolar {

/// One writable setting of the MPPT (see NumberKind). The state is only published once the charger has confirmed
/// the new value, so the entity always shows what the charger really uses.
class SmartSolarNumber : public number::Number, public Parented<SmartSolar> {
 public:
  // int on purpose: cv.enum() emits integer literals in the generated code
  void set_kind(int kind) { this->kind_ = static_cast<NumberKind>(kind); }
  NumberKind get_kind() const { return this->kind_; }

 protected:
  void control(float value) override { this->parent_->write_number(this->kind_, value); }

  NumberKind kind_{NUMBER_ABSORPTION_VOLTAGE};
};

}  // namespace smartsolar
}  // namespace esphome

#endif  // USE_NUMBER
