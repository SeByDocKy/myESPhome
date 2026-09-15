#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../ez1m.h"

namespace esphome {
namespace ez1m {

// Header-only: this entity is just a typed (kind) data holder, published to by the hub.
class EZ1MSensor : public sensor::Sensor, public Parented<EZ1MComponent> {
 public:
  void set_kind(EZ1MSensorType kind) { this->kind_ = kind; }
  EZ1MSensorType get_kind() const { return this->kind_; }

 protected:
  EZ1MSensorType kind_;
};

}  // namespace ez1m
}  // namespace esphome
