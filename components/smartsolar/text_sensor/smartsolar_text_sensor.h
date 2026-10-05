#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TEXT_SENSOR

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"
#include "../smartsolar.h"

namespace esphome {
namespace smartsolar {

/// One text entity of the MPPT, dispatched by its kind (see TextSensorKind).
class SmartSolarTextSensor : public text_sensor::TextSensor, public Parented<SmartSolar> {
 public:
  void set_kind(int kind) { this->kind_ = static_cast<TextSensorKind>(kind); }
  TextSensorKind get_kind() const { return this->kind_; }

 protected:
  TextSensorKind kind_{TEXT_STATE};
};

}  // namespace smartsolar
}  // namespace esphome

#endif  // USE_TEXT_SENSOR
