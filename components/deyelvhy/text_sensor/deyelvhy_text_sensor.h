#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TEXT_SENSOR

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/deyelvhy/deyelvhy.h"

#include <string>

namespace esphome::deyelvhy {

/// Overall state of the inverter (register 59): standby, selftest, normal, alarm, fault.
class DeyeLvHyStateTextSensor : public text_sensor::TextSensor, public Component, public DeyeLvHyItem {
 public:
  void dump_config() override;
  void parse_and_publish(std::span<const uint8_t> data) override;
};

/// Start time of a time-of-use slot, stored as HHMM in a register (1830 = 18:30), shown as "HH:MM".
class DeyeLvHyTimeTextSensor : public text_sensor::TextSensor, public Component, public DeyeLvHyItem {
 public:
  void dump_config() override;
  void parse_and_publish(std::span<const uint8_t> data) override;
};

}  // namespace esphome::deyelvhy

#endif  // USE_TEXT_SENSOR
