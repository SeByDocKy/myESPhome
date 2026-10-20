#include "faikin_number.h"

#include <cmath>

namespace esphome {
namespace faikin {

void FaikinNumber::control(float value) {
  // Published by the hub after the unit acknowledged the write and the read back confirmed it.
  this->parent_->control_demand(static_cast<int>(lroundf(value)));
}

}  // namespace faikin
}  // namespace esphome
