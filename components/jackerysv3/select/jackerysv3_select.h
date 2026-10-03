#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "../jackerysv3.h"

namespace esphome::jackerysv3 {

/// Selectable setting of the battery. The option index is the value written to the battery
/// (autoStandby: 0 = invalid, 1 = standby, 2 = on).
class JackerySV3Select : public select::Select, public Component, public Parented<JackerySV3Hub> {
 public:
  void dump_config() override;
  void set_kind(SelectKind kind) { this->kind_ = kind; }

 protected:
  void control(size_t index) override;

  SelectKind kind_{SelectKind::AUTO_STANDBY_MODE};
};

}  // namespace esphome::jackerysv3
