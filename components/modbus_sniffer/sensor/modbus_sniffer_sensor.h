#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../modbus_sniffer.h"
#include <vector>

namespace esphome {
namespace modbus_sniffer {

enum ValueType {
  U_WORD = 0,      // Unsigned 16-bit (1 registre)
  S_WORD = 1,      // Signed 16-bit (1 registre)
  U_DWORD = 2,     // Unsigned 32-bit (2 registres)
  S_DWORD = 3,     // Signed 32-bit (2 registres)
  U_DWORD_R = 4,   // Unsigned 32-bit reversed (2 registres, low word first)
  S_DWORD_R = 5,   // Signed 32-bit reversed (2 registres, low word first)
  FP32 = 6,        // Float 32-bit IEEE 754 (2 registres)
  FP32_R = 7,      // Float 32-bit reversed (2 registres, low word first)
};

class ModbusSnifferSensor : public sensor::Sensor, public Component {
 public:
  // Configuration (appelé depuis Python)
  void set_parent(ModbusSnifferHub *parent) { parent_ = parent; }
  void set_register_address(uint16_t address) { register_address_ = address; }
  void set_value_type(ValueType type) { value_type_ = type; }
  void set_register_type(RegisterType type) { register_type_ = type; }
  
  // Getters (utilisés par le hub)
  uint16_t get_register_address() const { return register_address_; }
  RegisterType get_register_type() const { return register_type_; }
  uint8_t get_register_count() const;
  
  // Appelé par le hub quand des données sont disponibles
  void process_data(uint16_t start_address, const std::vector<uint8_t> &data);
  
  // Component overrides
  void setup() override;
  void dump_config() override;

 protected:
  ModbusSnifferHub *parent_{nullptr};
  uint16_t register_address_{0};
  ValueType value_type_{U_WORD};
  RegisterType register_type_{HOLDING};
  
  // Décodage selon le type
  float decode_value(const std::vector<uint8_t> &data);
};

}  // namespace modbus_sniffer
}  // namespace esphome