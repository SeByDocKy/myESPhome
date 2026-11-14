#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../bluetti.h"

#include <utility>

namespace esphome::bluetti {

enum class TextKind : uint8_t {
  STRING,   // ASCII string (device type, battery type); the first character of a register is in its LOW byte
  SERIAL,   // 64 bit serial number, low word first, shown in decimal
  VERSION,  // firmware version packed as major * 10000 + minor * 100 + patch (2 registers, low word first)
  STATES,   // numeric state -> label (inverter status, working mode, PV type...)
  FLAGS,    // bit field -> the E codes that are set (inverter warnings and faults)
  HEX,      // bit mask of several registers shown in hex (battery protection / error / alarm masks)
};

class BluettiTextSensor : public text_sensor::TextSensor, public Component, public BluettiRegisterItem {
 public:
  void dump_config() override;
  void set_kind(TextKind kind) { this->kind_ = kind; }
  void add_state(uint16_t value, const char *label) { this->states_.emplace_back(value, label); }
  /// FLAGS: bit number (16 * register index + bit of the register) -> label
  void add_flag(uint8_t bit, const char *label) { this->flags_.emplace_back(bit, label); }
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  void publish_if_changed_(const std::string &value);

  TextKind kind_{TextKind::STRING};
  std::vector<std::pair<uint16_t, std::string>> states_;
  std::vector<std::pair<uint8_t, std::string>> flags_;
  std::string last_;
  bool published_{false};
};

}  // namespace esphome::bluetti
