#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include <vector>

namespace esphome {
namespace modbus_sniffer {

// Forward declaration
class ModbusSnifferSensor;
class ModbusSnifferBinarySensor;

enum ModbusFunction {
  READ_COILS = 0x01,
  READ_DISCRETE_INPUTS = 0x02,
  READ_HOLDING_REGISTERS = 0x03,
  READ_INPUT_REGISTERS = 0x04,
  WRITE_SINGLE_COIL = 0x05,
  WRITE_SINGLE_REGISTER = 0x06,
  WRITE_MULTIPLE_COILS = 0x0F,
  WRITE_MULTIPLE_REGISTERS = 0x10
};

enum RegisterType {
  HOLDING = 0,
  INPUT = 1,
};

// Structure pour stocker les données capturées
struct ModbusRegisterData {
  uint16_t start_address;
  std::vector<uint8_t> data;
  RegisterType type;
};

class ModbusSnifferHub : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration
  void set_slave_address(uint8_t address) { slave_address_ = address; }
  
  // Enregistrement des sensors
  void register_sensor(ModbusSnifferSensor *sensor);
  void register_binary_sensor(ModbusSnifferBinarySensor *sensor);
  
  // Méthode pour permettre aux sensors de récupérer les données
  bool get_register_data(uint16_t address, uint8_t count, RegisterType type, std::vector<uint8_t> &out_data);

 protected:
  // Buffer de réception UART
  std::vector<uint8_t> rx_buffer_;
  uint32_t last_byte_time_{0};
  uint32_t last_frame_time_{0};
  
  // Configuration
  uint8_t slave_address_{0};
  
  // Liste des sensors enregistrés
  std::vector<ModbusSnifferSensor*> sensors_;
  std::vector<ModbusSnifferBinarySensor*> binary_sensors_;
  
  // Dernière requête capturée
  uint8_t last_request_slave_{0};
  uint8_t last_request_function_{0};
  uint16_t last_request_address_{0};
  uint16_t last_request_count_{0};
  
  // Données capturées (cache temporaire)
  std::vector<ModbusRegisterData> captured_data_;
  
  // Timeout Modbus (temps entre les octets)
  static const uint32_t MODBUS_FRAME_TIMEOUT = 10; // ms - Réduit pour séparer requête/réponse
  static const uint32_t MODBUS_INTER_FRAME_DELAY = 3; // ms - Temps minimum entre deux trames
  
  // Méthodes de traitement des trames
  void process_frame();
  uint16_t calculate_crc(const uint8_t *data, uint8_t len);
  bool verify_crc(const std::vector<uint8_t> &frame);
  void parse_modbus_frame(const std::vector<uint8_t> &frame);
  void process_read_response(uint8_t slave, uint8_t function, const std::vector<uint8_t> &frame);
  void notify_sensors(uint16_t reg_addr, const std::vector<uint8_t> &data, RegisterType type);
  
  // Utilitaires
  std::string format_hex(const std::vector<uint8_t> &data);
};

}  // namespace modbus_sniffer
}  // namespace esphome
