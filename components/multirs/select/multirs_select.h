#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SELECT

#include "esphome/components/select/select.h"
#include "esphome/core/helpers.h"
#include "../multirs.h"

namespace esphome {
namespace multirs {

/// The Multi RS operating mode (register 0x0200). The state is only published once the device confirmed it.
class MultiRSSelect : public select::Select, public Parented<MultiRS> {
 public:
  // int on purpose: cv.enum() emits integer literals in the generated code
  void set_kind(int kind) { this->kind_ = static_cast<SelectKind>(kind); }
  SelectKind get_kind() const { return this->kind_; }

 protected:
  void control(size_t index) override { this->parent_->write_select(this->kind_, index); }
  SelectKind kind_{SELECT_MODE};
};

}  // namespace multirs
}  // namespace esphome

#endif  // USE_SELECT
