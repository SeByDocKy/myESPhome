#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../modbus_sniffer.h"
#include <vector>

namespace esphome {
namespace modbus_sniffer {

class ModbusSnifferBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  // Configuration (appelé depuis Python)
  void set_parent(ModbusSnifferHub *parent) { parent_ = parent; }
  void set_register_address(uint16_t address) { register_address_ = address; }
  void set_bitmask(uint16_t bitmask) { bitmask_ = bitmask; }
  void set_register_type(RegisterType type) { register_type_ = type; }
  
  // Getters (utilisés par le hub)
  uint16_t get_register_address() const { return register_address_; }
  RegisterType get_register_type() const { return register_type_; }
  uint16_t get_bitmask() const { return bitmask_; }
  uint8_t get_register_count() const { return 1; } // Toujours 1 registre pour binary sensor
  
  // Appelé par le hub quand des données sont disponibles
  void process_data(uint16_t start_address, const std::vector<uint8_t> &data);
  
  // Component overrides
  void setup() override;
  void dump_config() override;

 protected:
  ModbusSnifferHub *parent_{nullptr};
  uint16_t register_address_{0};
  uint16_t bitmask_{0x01}; // Par défaut bit 0
  RegisterType register_type_{HOLDING};
};

}  // namespace modbus_sniffer
}  // namespace esphome