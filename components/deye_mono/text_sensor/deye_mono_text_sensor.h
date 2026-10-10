#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../deye_mono.h"

#include <string>

namespace esphome::deye_mono {

/// Overall state of the inverter (register 59): standby, selftest, normal, alarm, fault.
class DeyeMonoStateTextSensor : public text_sensor::TextSensor, public Component, public DeyeMonoItem {
 public:
  void dump_config() override;
  void parse_and_publish(std::span<const uint8_t> data) override;
};

/// Start time of a time-of-use slot, stored as HHMM in a register (1830 = 18:30), shown as "HH:MM".
class DeyeMonoTimeTextSensor : public text_sensor::TextSensor, public Component, public DeyeMonoItem {
 public:
  void dump_config() override;
  void parse_and_publish(std::span<const uint8_t> data) override;
};

}  // namespace esphome::deye_mono
