#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TEXT_SENSOR

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"
#include "../multirs.h"

namespace esphome {
namespace multirs {

/// One text entity of the Multi RS (see TextSensorKind).
class MultiRSTextSensor : public text_sensor::TextSensor, public Parented<MultiRS> {
 public:
  // int on purpose: cv.enum() emits integer literals in the generated code
  void set_kind(int kind) { this->kind_ = static_cast<TextSensorKind>(kind); }
  TextSensorKind get_kind() const { return this->kind_; }

 protected:
  TextSensorKind kind_{TEXT_STATE};
};

}  // namespace multirs
}  // namespace esphome

#endif  // USE_TEXT_SENSOR
