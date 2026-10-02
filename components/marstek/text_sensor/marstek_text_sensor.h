#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../marstek.h"

#include <utility>

namespace esphome::marstek {

enum class TextKind : uint8_t {
  CHAR,    // ASCII string (device name, serial number, module firmware), cut at the first NUL
  MAC,     // 12 hex characters -> AA:BB:CC:DD:EE:FF
  IPV4,    // 4 bytes -> a.b.c.d
  STATES,  // numeric state -> label (inverter state)
  BITS,    // bit field -> comma separated list of the active bits (fault / alarm status)
};

class MarstekTextSensor : public text_sensor::TextSensor, public Component, public MarstekRegisterItem {
 public:
  void dump_config() override;
  void set_kind(TextKind kind) { this->kind_ = kind; }
  void add_state(uint16_t value, const char *label) { this->states_.emplace_back(value, label); }
  /// Bit `bit` of the field: register bit / 16 (lowest register first), bit % 16
  void add_bit(uint8_t bit, const char *name) { this->bits_.emplace_back(bit, name); }
  void parse_and_publish(const std::vector<uint8_t> &data) override;

 protected:
  void publish_if_changed_(const std::string &value);
  std::string decode_bytes_(const std::vector<uint8_t> &data, bool stop_at_nul) const;

  TextKind kind_{TextKind::CHAR};
  std::vector<std::pair<uint16_t, std::string>> states_;
  std::vector<std::pair<uint8_t, std::string>> bits_;
  std::string last_;
  bool published_{false};
};

/// "V147.6.112" / "V147.6.117.112" built from the EMS, BMS (and VMS) version registers.
class MarstekFirmwareTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void dump_config() override;
  void set_with_vms(bool with_vms) { this->with_vms_ = with_vms; }
  /// index: 0 = EMS, 1 = BMS, 2 = VMS
  void configure_dep(uint8_t index, uint16_t reg, uint8_t count, SensorValueType vtype, float scale,
                     uint16_t skip_updates);
  SensorItem *get_dep(uint8_t index) { return &this->deps_[index]; }

 protected:
  void recompute_();

  bool with_vms_{false};
  MarstekDepReader deps_[3];
  std::string last_;
  bool published_{false};
};

}  // namespace esphome::marstek
