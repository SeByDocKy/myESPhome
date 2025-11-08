#pragma once
#include "../emerson_r48.h"
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace emerson_r48 {

//class EmersonR48Switch : public switch_::Switch, public Component {
// public:
//  void setup() override;
//  void write_state(bool state) override;
//  void dump_config() override;
//};


class EmersonR48Switch : public switch_::Switch, public Component {
 public:
  void set_parent(EmersonR48Component *parent, int8_t functionCode) {
    this->parent_ = parent;
    this->functionCode_ = functionCode;
  };

  void setup() override;
  void write_state(bool state) override;
  void dump_config() override;

 protected:
  EmersonR48Component *parent_;
  int8_t functionCode_;
  
  //void control(float value) override;
};


} //namespace emerson_r48
} //namespace esphome