#pragma once
#include "../emerson_r48.h"
#include "esphome/core/component.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace emerson_r48 {

class EmersonR48Number : public number::Number, public Component {
 public:
  void set_parent(EmersonR48Component *parent, int8_t functionCode) {
    this->parent_ = parent;
    this->functionCode_ = functionCode;
  };
  // void setup() override;

 protected:
  EmersonR48Component *parent_;
  int8_t functionCode_;
  ESPPreferenceObject pref_;

  void control(float value) override;
};

}  // namespace emerson_r48
}  // namespace esphome
