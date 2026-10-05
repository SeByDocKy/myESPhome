#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SENSOR

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include "../smartsolar.h"

namespace esphome {
namespace smartsolar {

/// One numeric entity of the MPPT, dispatched by its kind (see SensorKind).
class SmartSolarSensor : public sensor::Sensor, public Parented<SmartSolar> {
 public:
  // int on purpose: cv.enum() emits integer literals in the generated code
  void set_kind(int kind) { this->kind_ = static_cast<SensorKind>(kind); }
  SensorKind get_kind() const { return this->kind_; }

 protected:
  SensorKind kind_{SENSOR_BATTERY_VOLTAGE};
};

}  // namespace smartsolar
}  // namespace esphome

#endif  // USE_SENSOR
