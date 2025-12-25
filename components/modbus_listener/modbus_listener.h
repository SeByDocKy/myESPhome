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
  void set_use_comma(bool use_comma) { use_comma_ = use_comma; }
  void set_use_hexa(bool use_hexa) { use_hexa_ = use_hexa; }
  void set_use_bracket(bool use_bracket) { use_bracket_ = use_bracket; }
  void set_use_quote(bool use_quote) { use_quote_ = use_quote; }
  
  // Enregistrement des text sensors
  void register_text_sensor(ModbusListenerTextSensor *sensor);
  
  // Méthodes pour récupérer les dernières trames capturées
  std::vector<uint8_t> get_last_tx_frame() const { return last_tx_frame_; }
  std::vector<uint8_t> get_last_rx_frame() const { return last_rx_frame_; }
  std::vector<uint8_t> get_last_frame() const { return last_frame_; }
  
  // Vérifier si des trames sont disponibles
  bool has_tx_frame() const { return !last_tx_frame_.empty(); }
  bool has_rx_frame() const { return !last_rx_frame_.empty(); }
  bool has_frame() const { return !last_frame_.empty(); }

 protected:
  // Buffer de réception UART
  std::vector<uint8_t> rx_buffer_;
  uint32_t last_byte_time_{0};
  uint32_t last_frame_time_{0};
  
  // Configuration
  uint8_t slave_address_{0};
  bool use_comma_{false};
  bool use_hexa_{false};
  bool use_bracket_{false};
  bool use_quote_{false};
  
  // Liste des text sensors enregistrés
  std::vector<ModbusListenerTextSensor*> text_sensors_;
  
  // Dernières trames capturées (pour accès externe)
  std::vector<uint8_t> last_tx_frame_;  // Dernière requête (client → serveur)
  std::vector<uint8_t> last_rx_frame_;  // Dernière réponse (serveur → client)
  std::vector<uint8_t> last_frame_;     // Dernière trame (TX ou RX)
  
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
