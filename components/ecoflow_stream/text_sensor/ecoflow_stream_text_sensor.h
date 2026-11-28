#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../ecoflow_stream.h"

#include <string>
#include <utility>
#include <vector>

namespace esphome::ecoflow_stream {

enum class TextKind : uint8_t {
  STRING,   // ASCII string, first character of a register in its high byte
  VERSION,  // UINT32 shown as a.b.c.d, one byte per field
  DECIMAL,  // UINT32 identifier (a float sensor would lose digits)
  STATES,   // numeric state -> label
  CODES,    // list of fault code registers: the non zero ones
};

class EcoflowStreamTextSensor : public text_sensor::TextSensor, public Component, public EcoflowStreamRegisterItem {
 public:
  void dump_config() override;
  void set_kind(TextKind kind) { this->kind_ = kind; }
  void add_state(uint16_t value, const char *label) { this->states_.emplace_back(value, label); }
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  TextKind kind_{TextKind::STRING};
  std::vector<std::pair<uint16_t, const char *>> states_;
  std::string last_;
  bool published_{false};
};

}  // namespace esphome::ecoflow_stream
