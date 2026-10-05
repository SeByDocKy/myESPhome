#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BINARY_SENSOR

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/helpers.h"
#include "../smartsolar.h"

namespace esphome {
namespace smartsolar {

/// One binary entity of the MPPT, dispatched by its kind (see BinarySensorKind).
class SmartSolarBinarySensor : public binary_sensor::BinarySensor, public Parented<SmartSolar> {
 public:
  void set_kind(int kind) { this->kind_ = static_cast<BinarySensorKind>(kind); }
  BinarySensorKind get_kind() const { return this->kind_; }

 protected:
  BinarySensorKind kind_{BINARY_RELAY};
};

}  // namespace smartsolar
}  // namespace esphome

#endif  // USE_BINARY_SENSOR
