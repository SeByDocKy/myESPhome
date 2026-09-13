#include "number.h"

namespace esphome {
namespace cmt2300a {

void CMT2300APALevelNumber::control(float value) {
  // set_pa_level() already republishes the exact state via pa_level_number_
  // (see cmt2300a.cpp) -- no need for an extra publish_state() here.
  if (this->parent_ != nullptr) {
    this->parent_->set_pa_level(static_cast<int8_t>(value));
  }
}

}  // namespace cmt2300a
}  // namespace esphome
