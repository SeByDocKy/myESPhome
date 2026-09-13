#include "number.h"

namespace esphome {
namespace cmt2300a {

void CMT2300APALevelNumber::control(float value) {
  // set_pa_level() republie déjà l'état exact via pa_level_number_ (voir
  // cmt2300a.cpp) -- pas besoin de publish_state() ici en plus.
  if (this->parent_ != nullptr) {
    this->parent_->set_pa_level(static_cast<int8_t>(value));
  }
}

}  // namespace cmt2300a
}  // namespace esphome
