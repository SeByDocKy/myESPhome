#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../anker_solix.h"

#include <utility>

namespace esphome::anker_solix {

enum class TextKind : uint8_t {
  CHAR,     // ASCII string (serial number, software version, part number), cut at the first NUL
  STATES,   // numeric state -> label (battery status)
  PRODUCT,  // friendly product name derived from the serial number
};

class AnkerSolixTextSensor : public text_sensor::TextSensor, public Component, public AnkerSolixRegisterItem {
 public:
  void dump_config() override;
  void set_kind(TextKind kind) { this->kind_ = kind; }
  void add_state(uint16_t value, const char *label) { this->states_.emplace_back(value, label); }
  /// PRODUCT: product code (from the serial number) -> name, and what to show for an unknown code
  void add_product(const char *code, const char *name) { this->products_.emplace_back(code, name); }
  void set_default_name(const char *name) { this->default_name_ = name; }
  void set_model_name(const char *name) { this->model_name_ = name; }
  void parse_and_publish(std::span<const uint8_t> data) override;

 protected:
  void publish_if_changed_(const std::string &value);
  std::string decode_text_(std::span<const uint8_t> data) const;

  TextKind kind_{TextKind::CHAR};
  std::vector<std::pair<uint16_t, std::string>> states_;
  std::vector<std::pair<std::string, std::string>> products_;
  std::string default_name_;
  std::string model_name_;
  std::string last_;
  bool published_{false};
  bool warned_{false};
};

}  // namespace esphome::anker_solix
