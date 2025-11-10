#pragma once
#include "../emerson_r48.h"
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace emerson_r48 {

class EmersonR48MaxCurrentOutput : public output::FloatOutput, public Component {
 public:
  void set_parent(EmersonR48Component *parent) {
    this->parent_ = parent;
  };

 protected:
  EmersonR48Component *parent_;
  void write_state(float value) override;
};

}  // namespace emerson_r48
}  // namespace esphome
