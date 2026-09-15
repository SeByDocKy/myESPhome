#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../ez1m.h"

namespace esphome {
namespace ez1m {

// Header-only: typed (kind) data holder, published to by the hub.
class EZ1MTextSensor : public text_sensor::TextSensor, public Parented<EZ1MComponent> {
 public:
  void set_kind(EZ1MTextSensorType kind) { this->kind_ = kind; }
  EZ1MTextSensorType get_kind() const { return this->kind_; }

 protected:
  EZ1MTextSensorType kind_;
};

}  // namespace ez1m
}  // namespace esphome
