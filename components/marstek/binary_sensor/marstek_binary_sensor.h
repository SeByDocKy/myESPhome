#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../marstek.h"

namespace esphome::marstek {

enum class BinaryKind : uint8_t {
  BOOL,      // register != 0
  BITS_ANY,  // at least one bit set in the registers (fault / alarm active)
};

/// Binary sensor fed by a register. The "modbus_connection" entity is the same class without a register: the hub
/// drives it (see MarstekHub::set_connection_sensor()).
class MarstekBinarySensor : public binary_sensor::BinarySensor, public Component, public MarstekRegisterItem {
 public:
  void dump_config() override;
  void set_kind(BinaryKind kind) { this->kind_ = kind; }
  void parse_and_publish(const std::vector<uint8_t> &data) override;

 protected:
  BinaryKind kind_{BinaryKind::BOOL};
};

}  // namespace esphome::marstek
