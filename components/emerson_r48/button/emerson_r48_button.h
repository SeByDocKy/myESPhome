#pragma once
#include "../emerson_r48.h"
#include "esphome/core/component.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace emerson_r48 {

class EmersonR48Button : public button::Button, public Component {
 public:
  void set_parent(EmersonR48Component *parent) { this->parent_ = parent; };

 protected:
  EmersonR48Component *parent_;

  void press_action() override;
};

}  // namespace emesron_r48
}  // namespace esphome
