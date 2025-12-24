#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include <vector>

namespace esphome {
namespace modbus_listener {

// Forward declaration
class ModbusListenerTextSensor;

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

enum FrameType {
  FRAME_REQUEST = 0,
  FRAME_RESPONSE = 1,
};

class ModbusListenerHub : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration
  void set_slave_address(uint8_t address) { slave_address_ = address; }
  
  // Enregistrement des text sensors
  void register_text_sensor(ModbusListenerTextSensor *sensor);

 protected:
  // Buffer de réception UART
  std::vector<uint8_t> rx_buffer_;
  uint32_t last_byte_time_{0};
  uint32_t last_frame_time_{0};
  
  // Configuration
  uint8_t slave_address_{0};
  
  // Liste des text sensors enregistrés
  std::vector<ModbusListenerTextSensor*> text_sensors_;
  
  // Timeouts
  static const uint32_t MODBUS_FRAME_TIMEOUT = 10; // ms
  static const uint32_t MODBUS_INTER_FRAME_DELAY = 3; // ms
  
  // Méthodes de traitement des trames
  void process_frame();
  bool is_frame_complete();
  uint16_t calculate_crc(const uint8_t *data, uint8_t len);
  bool verify_crc(const std::vector<uint8_t> &frame);
  FrameType detect_frame_type(const std::vector<uint8_t> &frame);
  void notify_text_sensors(const std::vector<uint8_t> &frame, FrameType type);
  
  // Utilitaires
  std::string format_hex(const std::vector<uint8_t> &data);
};

}  // namespace modbus_listener
}  // namespace esphome