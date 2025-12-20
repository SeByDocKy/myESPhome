#include "modbus_sniffer.h"
#ifdef USE_SENSOR
#include "sensor/modbus_sniffer_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "binary_sensor/modbus_sniffer_binary_sensor.h
#endif
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_sniffer {

static const char *const TAG = "modbus_sniffer";

void ModbusSnifferHub::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Modbus Sniffer Hub...");
}

void ModbusSnifferHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus Sniffer Hub:");
  if (slave_address_ > 0) {
    ESP_LOGCONFIG(TAG, "  Slave Address Filter: 0x%02X", slave_address_);
  }
  ESP_LOGCONFIG(TAG, "  Registered Sensors: %d", sensors_.size());
  ESP_LOGCONFIG(TAG, "  Registered Binary Sensors: %d", binary_sensors_.size());
}

void ModbusSnifferHub::register_sensor(ModbusSnifferSensor *sensor) {
  sensors_.push_back(sensor);
  ESP_LOGD(TAG, "Registered sensor at address 0x%04X", sensor->get_register_address());
}

void ModbusSnifferHub::register_binary_sensor(ModbusSnifferBinarySensor *sensor) {
  binary_sensors_.push_back(sensor);
  ESP_LOGD(TAG, "Registered binary sensor at address 0x%04X with bitmask 0x%04X", 
           sensor->get_register_address(), sensor->get_bitmask());
}

void ModbusSnifferHub::loop() {
  const uint32_t now = millis();
  
  // Lecture des données disponibles sur UART
  while (available()) {
    uint8_t byte;
    read_byte(&byte);
    rx_buffer_.push_back(byte);
    last_byte_time_ = now;
  }
  
  // Détection de fin de trame (timeout Modbus)
  if (!rx_buffer_.empty() && (now - last_byte_time_) > MODBUS_FRAME_TIMEOUT) {
    process_frame();
    rx_buffer_.clear();
  }
}

void ModbusSnifferHub::process_frame() {
  // Trame minimale: Slave(1) + Function(1) + Data(≥0) + CRC(2) = minimum 4 bytes
  if (rx_buffer_.size() < 4) {
    return;
  }
  
  // Vérification CRC
  if (!verify_crc(rx_buffer_)) {
    ESP_LOGW(TAG, "Invalid CRC - Frame: %s", format_hex(rx_buffer_).c_str());
    return;
  }
  
  parse_modbus_frame(rx_buffer_);
}

uint16_t ModbusSnifferHub::calculate_crc(const uint8_t *data, uint8_t len) {
  uint16_t crc = 0xFFFF;
  
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  
  return crc;
}

bool ModbusSnifferHub::verify_crc(const std::vector<uint8_t> &frame) {
  if (frame.size() < 4) return false;
  
  uint16_t calculated_crc = calculate_crc(frame.data(), frame.size() - 2);
  uint16_t received_crc = (frame[frame.size() - 1] << 8) | frame[frame.size() - 2];
  
  return calculated_crc == received_crc;
}

void ModbusSnifferHub::parse_modbus_frame(const std::vector<uint8_t> &frame) {
  uint8_t slave_addr = frame[0];
  uint8_t function = frame[1];
  
  // Filtre par adresse esclave si configuré
  if (slave_address_ > 0 && slave_addr != slave_address_) {
    return;
  }
  
  // Détection des erreurs Modbus (bit 7 = 1)
  if (function & 0x80) {
    uint8_t error_code = frame[2];
    ESP_LOGW(TAG, "MODBUS ERROR - Slave: 0x%02X, Function: 0x%02X, Error: 0x%02X", 
             slave_addr, function & 0x7F, error_code);
    return;
  }
  
  ESP_LOGV(TAG, "Frame - Slave: 0x%02X, Func: 0x%02X, Size: %d", 
           slave_addr, function, frame.size());
  
  // Analyse selon la fonction
  switch (function) {
    case READ_HOLDING_REGISTERS:
    case READ_INPUT_REGISTERS:
      if (frame.size() == 8) {
        // REQUÊTE: Slave + Func + StartAddr(2) + Count(2) + CRC(2) = 8 bytes
        uint16_t start_addr = (frame[2] << 8) | frame[3];
        uint16_t count = (frame[4] << 8) | frame[5];
        
        ESP_LOGD(TAG, "REQUEST - Func: 0x%02X, Start: 0x%04X, Count: %d", 
                 function, start_addr, count);
        
        // Mémoriser la requête pour associer la réponse
        last_request_slave_ = slave_addr;
        last_request_function_ = function;
        last_request_address_ = start_addr;
        last_request_count_ = count;
        
      } else if (frame.size() >= 5 && frame[2] <= 250) {
        // RÉPONSE: Slave + Func + ByteCount + Data + CRC
        process_read_response(slave_addr, function, frame);
      }
      break;
      
    case WRITE_SINGLE_REGISTER:
      if (frame.size() >= 6) {
        uint16_t reg_addr = (frame[2] << 8) | frame[3];
        uint16_t value = (frame[4] << 8) | frame[5];
        ESP_LOGD(TAG, "WRITE_SINGLE - Reg: 0x%04X = %d (0x%04X)", reg_addr, value, value);
      }
      break;
      
    case WRITE_MULTIPLE_REGISTERS:
      if (frame.size() >= 6) {
        uint16_t start_reg = (frame[2] << 8) | frame[3];
        uint16_t count = (frame[4] << 8) | frame[5];
        ESP_LOGD(TAG, "WRITE_MULTIPLE - Start: 0x%04X, Count: %d", start_reg, count);
      }
      break;
      
    default:
      ESP_LOGV(TAG, "Other function 0x%02X: %s", function, format_hex(frame).c_str());
      break;
  }
}

void ModbusSnifferHub::process_read_response(uint8_t slave, uint8_t function, 
                                             const std::vector<uint8_t> &frame) {
  uint8_t byte_count = frame[2];
  
  if (frame.size() < (size_t)(5 + byte_count)) {
    ESP_LOGW(TAG, "Invalid response frame size");
    return;
  }
  
  // Vérifier que c'est une réponse à notre dernière requête
  if (slave != last_request_slave_ || function != last_request_function_) {
    ESP_LOGV(TAG, "Response doesn't match last request");
    return;
  }
  
  RegisterType reg_type = (function == READ_HOLDING_REGISTERS) ? HOLDING : INPUT;
  
  ESP_LOGD(TAG, "RESPONSE - Type: %s, Start: 0x%04X, Bytes: %d", 
           reg_type == HOLDING ? "Holding" : "Input",
           last_request_address_, byte_count);
  
  // Extraire les données brutes (sans Slave, Func, ByteCount, CRC)
  std::vector<uint8_t> data;
  for (uint8_t i = 0; i < byte_count; i++) {
    data.push_back(frame[3 + i]);
  }
  
  // Stocker dans le cache
  ModbusRegisterData reg_data;
  reg_data.start_address = last_request_address_;
  reg_data.data = data;
  reg_data.type = reg_type;
  captured_data_.push_back(reg_data);
  
  // Limiter la taille du cache (garder les 10 dernières captures)
  if (captured_data_.size() > 10) {
    captured_data_.erase(captured_data_.begin());
  }
  
  // Notifier les sensors
  notify_sensors(last_request_address_, data, reg_type);
}

void ModbusSnifferHub::notify_sensors(uint16_t reg_addr, const std::vector<uint8_t> &data, 
                                      RegisterType type) {
  uint16_t reg_count = data.size() / 2; // Nombre de registres 16-bit
  
  // Notifier les sensors normaux
  for (auto *sensor : sensors_) {
    if (sensor->get_register_type() != type) {
      continue;
    }
    
    uint16_t sensor_addr = sensor->get_register_address();
    uint8_t sensor_count = sensor->get_register_count();
    
    // Vérifier si le registre du sensor est dans cette réponse
    if (sensor_addr >= reg_addr && sensor_addr < (reg_addr + reg_count)) {
      uint16_t offset = (sensor_addr - reg_addr) * 2; // Offset en bytes
      
      // Vérifier qu'on a assez de données
      if ((offset + sensor_count * 2) <= data.size()) {
        std::vector<uint8_t> sensor_data(data.begin() + offset, 
                                         data.begin() + offset + sensor_count * 2);
        sensor->process_data(sensor_addr, sensor_data);
      }
    }
  }
  
  // Notifier les binary sensors
  for (auto *binary_sensor : binary_sensors_) {
    if (binary_sensor->get_register_type() != type) {
      continue;
    }
    
    uint16_t sensor_addr = binary_sensor->get_register_address();
    
    // Vérifier si le registre du binary sensor est dans cette réponse
    if (sensor_addr >= reg_addr && sensor_addr < (reg_addr + reg_count)) {
      uint16_t offset = (sensor_addr - reg_addr) * 2; // Offset en bytes
      
      // Vérifier qu'on a assez de données (2 bytes = 1 registre)
      if ((offset + 2) <= data.size()) {
        std::vector<uint8_t> sensor_data(data.begin() + offset, 
                                         data.begin() + offset + 2);
        binary_sensor->process_data(sensor_addr, sensor_data);
      }
    }
  }
}

bool ModbusSnifferHub::get_register_data(uint16_t address, uint8_t count, 
                                         RegisterType type, std::vector<uint8_t> &out_data) {
  // Chercher dans le cache
  for (const auto &cached : captured_data_) {
    if (cached.type != type) continue;
    
    uint16_t cached_count = cached.data.size() / 2;
    
    if (address >= cached.start_address && 
        address < (cached.start_address + cached_count)) {
      uint16_t offset = (address - cached.start_address) * 2;
      
      if ((offset + count * 2) <= cached.data.size()) {
        out_data.assign(cached.data.begin() + offset, 
                       cached.data.begin() + offset + count * 2);
        return true;
      }
    }
  }
  
  return false;
}

std::string ModbusSnifferHub::format_hex(const std::vector<uint8_t> &data) {
  char buf[data.size() * 3 + 1];
  char *p = buf;
  for (uint8_t byte : data) {
    p += sprintf(p, "%02X ", byte);
  }
  if (p > buf) *(p-1) = '\0';
  return std::string(buf);
}

}  // namespace modbus_sniffer
}  // namespace esphome
