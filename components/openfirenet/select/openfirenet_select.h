#pragma once

#include "esphome/core/defines.h"
#ifdef USE_SELECT
#include "esphome/core/helpers.h"
#include "esphome/components/select/select.h"
#include "../openfirenet.h"

namespace esphome {
namespace openfirenet {

class OpenFireNetModeSelect : public select::Select, public Parented<OpenFireNetComponent> {
 protected:
  void control(size_t index) override;
  // The string overload of Select::control() resolves the index and forwards to the one above.
  using select::Select::control;
};

}  // namespace openfirenet
}  // namespace esphome
#endif
