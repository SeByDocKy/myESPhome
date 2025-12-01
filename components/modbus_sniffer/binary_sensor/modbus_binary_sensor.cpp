#include "modbus_sniffer_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_sniffer {

static const char *const TAG = "modbus_sniffer.binary_sensor";

void ModbusSnifferBinarySensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Modbus Sniffer Binary Sensor '%s'...", get_name().c_str());
}

void ModbusSnifferBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Modbus Sniffer Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register Address: 0x%04X", register_address_);
  ESP_LOGCONFIG(TAG, "  Register Type: %s", register_type_ == HOLDING ? "Holding" : "Input");
  ESP_LOGCONFIG(TAG, "  Bitmask: 0x%04X", bitmask_);
  
  // Afficher quels bits sont surveillés
  std::string bits_str = "";
  for (int i = 0; i < 16; i++) {
    if (bitmask_ & (1 << i)) {
      if (!bits_str.empty()) bits_str += ", ";
      bits_str += "bit " + std::to_string(i);
    }
  }
  if (!bits_str.empty()) {
    ESP_LOGCONFIG(TAG, "  Monitoring: %s", bits_str.c_str());
  }
}

void ModbusSnifferBinarySensor::process_data(uint16_t start_address, const std::vector<uint8_t> &data) {
  // Vérifier qu'on a au moins 2 bytes (1 registre 16-bit)
  if (data.size() < 2) {
    ESP_LOGW(TAG, "Insufficient data for binary sensor '%s' (got %d bytes, need 2)", 
             get_name().c_str(), data.size());
    return;
  }
  
  // Lire le registre 16-bit (Big Endian)
  uint16_t register_value = (data[0] << 8) | data[1];
  
  // Appliquer le bitmask
  bool state = (register_value & bitmask_) != 0;
  
  ESP_LOGI(TAG, "Binary Sensor '%s' [0x%04X] = %s (register: 0x%04X, mask: 0x%04X)", 
           get_name().c_str(), 
           register_address_, 
           state ? "ON" : "OFF",
           register_value,
           bitmask_);
  
  // Publier l'état dans Home Assistant
  publish_state(state);
}

}  // namespace modbus_sniffer
}  // namespace esphome