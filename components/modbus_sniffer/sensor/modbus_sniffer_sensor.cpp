#include "modbus_sniffer_sensor.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace modbus_sniffer {

static const char *const TAG = "modbus_sniffer.sensor";

void ModbusSnifferSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Modbus Sniffer Sensor '%s'...", get_name().c_str());
}

void ModbusSnifferSensor::dump_config() {
  LOG_SENSOR("", "Modbus Sniffer Sensor", this);
  ESP_LOGCONFIG(TAG, "  Register Address: 0x%04X", register_address_);
  ESP_LOGCONFIG(TAG, "  Register Type: %s", register_type_ == HOLDING ? "Holding" : "Input");
  ESP_LOGCONFIG(TAG, "  Value Type: %s", 
    value_type_ == U_WORD ? "U_WORD" :
    value_type_ == S_WORD ? "S_WORD" :
    value_type_ == U_DWORD ? "U_DWORD" :
    value_type_ == S_DWORD ? "S_DWORD" :
    value_type_ == U_DWORD_R ? "U_DWORD_R" :
    value_type_ == S_DWORD_R ? "S_DWORD_R" :
    value_type_ == FP32 ? "FP32" :
    value_type_ == FP32_R ? "FP32_R" : "Unknown");
  ESP_LOGCONFIG(TAG, "  Register Count: %d", get_register_count());
}

uint8_t ModbusSnifferSensor::get_register_count() const {
  switch (value_type_) {
    case U_WORD:
    case S_WORD:
      return 1;
    case U_DWORD:
    case S_DWORD:
    case U_DWORD_R:
    case S_DWORD_R:
    case FP32:
    case FP32_R:
      return 2;
    default:
      return 1;
  }
}

void ModbusSnifferSensor::process_data(uint16_t start_address, const std::vector<uint8_t> &data) {
  // Vérifier qu'on a assez de données
  uint8_t required_bytes = get_register_count() * 2;
  if (data.size() < required_bytes) {
    ESP_LOGW(TAG, "Insufficient data for sensor '%s' (got %d bytes, need %d)", 
             get_name().c_str(), data.size(), required_bytes);
    return;
  }
  
  // Décoder la valeur
  float value = decode_value(data);
  
  ESP_LOGI(TAG, "Sensor '%s' [0x%04X] = %.3f", 
           get_name().c_str(), register_address_, value);
  
  // Publier dans Home Assistant
  publish_state(value);
}

float ModbusSnifferSensor::decode_value(const std::vector<uint8_t> &data) {
  // Premier registre (16 bits)
  uint16_t reg1 = (data[0] << 8) | data[1];
  
  switch (value_type_) {
    case U_WORD:
      // Unsigned 16-bit
      return static_cast<float>(reg1);
      
    case S_WORD:
      // Signed 16-bit
      return static_cast<float>(static_cast<int16_t>(reg1));
      
    case U_DWORD: {
      // Unsigned 32-bit: High word first
      if (data.size() < 4) return 0;
      uint32_t val = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                     ((uint32_t)data[2] << 8) | data[3];
      return static_cast<float>(val);
    }
    
    case S_DWORD: {
      // Signed 32-bit: High word first
      if (data.size() < 4) return 0;
      uint32_t val = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                     ((uint32_t)data[2] << 8) | data[3];
      return static_cast<float>(static_cast<int32_t>(val));
    }
    
    case U_DWORD_R: {
      // Unsigned 32-bit reversed: Low word first
      if (data.size() < 4) return 0;
      uint32_t val = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16) |
                     ((uint32_t)data[0] << 8) | data[1];
      return static_cast<float>(val);
    }
    
    case S_DWORD_R: {
      // Signed 32-bit reversed: Low word first
      if (data.size() < 4) return 0;
      uint32_t val = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16) |
                     ((uint32_t)data[0] << 8) | data[1];
      return static_cast<float>(static_cast<int32_t>(val));
    }
    
    case FP32: {
      // Float 32-bit IEEE 754: High word first
      if (data.size() < 4) return 0;
      uint32_t val = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                     ((uint32_t)data[2] << 8) | data[3];
      float result;
      memcpy(&result, &val, sizeof(float));
      return result;
    }
    
    case FP32_R: {
      // Float 32-bit IEEE 754 reversed: Low word first
      if (data.size() < 4) return 0;
      uint32_t val = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16) |
                     ((uint32_t)data[0] << 8) | data[1];
      float result;
      memcpy(&result, &val, sizeof(float));
      return result;
    }
    
    default:
      ESP_LOGW(TAG, "Unknown value type: %d", value_type_);
      return 0;
  }
}

}  // namespace modbus_sniffer
}  // namespace esphome