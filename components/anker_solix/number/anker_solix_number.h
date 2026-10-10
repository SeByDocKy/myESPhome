#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "../anker_solix.h"

namespace esphome::anker_solix {

/// Power setpoint of the battery (register 10071, INT32): charge < 0, discharge > 0, in W. It is only applied by the
/// battery in the "Third-Party Controlled" mode. The register is write-only here (it is not polled): the number
/// shows the last value sent, limited to what the battery can do.
class AnkerSolixPowerNumber : public number::Number, public Component, public Parented<AnkerSolixHub> {
 public:
  void dump_config() override;

 protected:
  void control(float value) override;
};

/// State of charge limit (registers 60000..60002, %). A write is checked against the capability mask of the device
/// and the ordering rules of the official integration (see AnkerSolixHub::check_soc_write()).
class AnkerSolixSocNumber : public number::Number,
                       public Component,
                       public AnkerSolixRegisterItem,
                       public Parented<AnkerSolixHub> {
 public:
  void dump_config() override;
  void set_rules(uint8_t capability_bit, bool needs_backup_enable) {
    this->capability_bit_ = capability_bit;
    this->needs_backup_enable_ = needs_backup_enable;
  }
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  void control(float value) override;

  uint8_t capability_bit_{0};
  bool needs_backup_enable_{false};
};

}  // namespace esphome::anker_solix
