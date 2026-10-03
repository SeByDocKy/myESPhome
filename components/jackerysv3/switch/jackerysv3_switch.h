#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "../jackerysv3.h"

namespace esphome::jackerysv3 {

/// Switch of the battery (AC socket, auto standby allowed) or of one of its smart plugs.
/// The state shown comes from the battery's reports; a command is acknowledged optimistically and corrected by
/// the next report.
class JackerySV3Switch : public switch_::Switch, public Component, public Parented<JackerySV3Hub> {
 public:
  void dump_config() override;
  void set_kind(SwitchKind kind) { this->kind_ = kind; }
  /// Plug slot (0..9) for the PLUG kind
  void set_index(uint8_t index) { this->index_ = index; }

 protected:
  void write_state(bool state) override;

  SwitchKind kind_{SwitchKind::AC_SOCKET};
  uint8_t index_{0};
};

}  // namespace esphome::jackerysv3
