#pragma once

#include "esphome/components/number/number.h"
#include "../offsr.h"

namespace esphome {
namespace offsr {

class ManualLevelNumber : public number::Number, public Parented<OFFSRComponent> {
 public:
  ManualLevelNumber() = default;
  
/*   void setup() override;
  void dump_config() override; */

 protected:
  void control(float value) override;
};

}  // namespace offsr
}  // namespace esphome
