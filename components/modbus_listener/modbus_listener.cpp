#include "modbus_listener.h"
#include "esphome/core/log.h"

#ifdef USE_TEXT_SENSOR
#include "text_sensor/modbus_listener_text_sensor.h"
#endif

namespace esphome {
namespace modbus_listener {

static const char *const TAG = "modbus_listener";

void ModbusListenerHub::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Modbus Listener Hub...");
  
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "UART parent is null!");
    this->mark_failed();
    return;
  }
  
  ESP_LOGCONFIG(TAG, "UART initialized successfully");
}

void ModbusListenerHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus Listener Hub:");
  if (slave_address_ > 0) {
    ESP_LOGCONFIG(TAG, "  Slave Address Filter: 0x%02X", slave_address_);
  }
  ESP_LOGCONFIG(TAG, "  Formatting Options:");
  ESP_LOGCONFIG(TAG, "    Use Comma: %s", use_comma_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "    Use Hexa Prefix: %s", use_hexa_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "    Use Brackets: %s", use_bracket_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "    Use Quotes: %s", use_quote_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Registered Text Sensors: %d", text_sensors_.size());
}

void ModbusListenerHub::register_text_sensor(ModbusListenerTextSensor *sensor) {
#ifdef USE_TEXT_SENSOR
  text_sensors_.push_back(sensor);
  ESP_LOGD(TAG, "Registered text sensor");
#endif
}

void ModbusListenerHub::loop() {
  const uint32_t now = millis();
  
  // Lecture des données disponibles sur UART
  while (available()) {
    uint8_t byte;
    if (!read_byte(&byte)) {
      ESP_LOGW(TAG, "Failed to read byte from UART");
      break;
    }
    
    // Début d'une nouvelle trame
    if (rx_buffer_.empty() && (now - last_frame_time_) > MODBUS_INTER_FRAME_DELAY) {
      ESP_LOGVV(TAG, ">>> Start of new frame");
    }
    
    rx_buffer_.push_back(byte);
    last_byte_time_ = now;
    
    ESP_LOGVV(TAG, "RX byte: 0x%02X (buffer size: %d)", byte, rx_buffer_.size());
    
    // Vérifier si on a une trame complète
    if (is_frame_complete()) {
      ESP_LOGD(TAG, "Frame complete (smart detection), processing %d bytes", rx_buffer_.size());
      process_frame();
      rx_buffer_.clear();
      last_frame_time_ = now;
    }
  }
  
  // Fallback: timeout
  if (!rx_buffer_.empty() && (now - last_byte_time_) > MODBUS_FRAME_TIMEOUT) {
    ESP_LOGD(TAG, "Frame complete (timeout), processing %d bytes", rx_buffer_.size());
    process_frame();
    rx_buffer_.clear();
    last_frame_time_ = now;
  }
}

void ModbusListenerHub::process_frame() {
  if (rx_buffer_.size() < 4) {
    return;
  }
  
  // Vérification CRC
  if (!verify_crc(rx_buffer_)) {
    ESP_LOGW(TAG, "Invalid CRC - Frame: %s", format_hex(rx_buffer_).c_str());
    return;
  }
  
  uint8_t slave_addr = rx_buffer_[0];
  
  // Filtre par adresse esclave si configuré
  if (slave_address_ > 0 && slave_addr != slave_address_) {
    return;
  }
  
  // Détecter le type de trame (requête ou réponse)
  FrameType frame_type = detect_frame_type(rx_buffer_);
  
  ESP_LOGI(TAG, "Frame detected - Type: %s, Size: %d bytes", 
           frame_type == FRAME_REQUEST ? "REQUEST" : "RESPONSE",
           rx_buffer_.size());
  ESP_LOGD(TAG, "  Data: %s", format_hex(rx_buffer_).c_str());
  
  // Notifier les text sensors
  notify_text_sensors(rx_buffer_, frame_type);
}

bool ModbusListenerHub::is_frame_complete() {
  if (rx_buffer_.size() < 4) {
    return false;
  }
  
  uint8_t function = rx_buffer_[1];
  
  // Gestion des erreurs Modbus (bit 7 = 1)
  if (function & 0x80) {
    if (rx_buffer_.size() >= 5) {
      return verify_crc(rx_buffer_);
    }
    return false;
  }
  
  // Calcul de la longueur attendue selon la fonction
  switch (function) {
    case READ_HOLDING_REGISTERS:
    case READ_INPUT_REGISTERS:
      // Requête: 8 bytes
      if (rx_buffer_.size() >= 8) {
        if (verify_crc(rx_buffer_)) {
          return true;
        }
      }
      
      // Réponse: Variable selon ByteCount
      if (rx_buffer_.size() >= 3) {
        uint8_t byte_count = rx_buffer_[2];
        if (byte_count > 0 && byte_count <= 250) {
          size_t expected_length = 3 + byte_count + 2;
          if (rx_buffer_.size() >= expected_length) {
            return verify_crc(rx_buffer_);
          }
        }
      }
      return false;
      
    case WRITE_SINGLE_COIL:
    case WRITE_SINGLE_REGISTER:
      if (rx_buffer_.size() >= 8) {
        return verify_crc(rx_buffer_);
      }
      return false;
      
    case WRITE_MULTIPLE_COILS:
    case WRITE_MULTIPLE_REGISTERS:
      if (rx_buffer_.size() >= 7) {
        uint8_t byte_count = rx_buffer_[6];
        size_t expected_length = 7 + byte_count + 2;
        if (rx_buffer_.size() >= expected_length) {
          return verify_crc(rx_buffer_);
        }
      }
      return false;
      
    default:
      return false;
  }
}

uint16_t ModbusListenerHub::calculate_crc(const uint8_t *data, uint8_t len) {
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

bool ModbusListenerHub::verify_crc(const std::vector<uint8_t> &frame) {
  if (frame.size() < 4) return false;
  
  uint16_t calculated_crc = calculate_crc(frame.data(), frame.size() - 2);
  uint16_t received_crc = (frame[frame.size() - 1] << 8) | frame[frame.size() - 2];
  
  return calculated_crc == received_crc;
}

FrameType ModbusListenerHub::detect_frame_type(const std::vector<uint8_t> &frame) {
  if (frame.size() < 3) {
    return FRAME_REQUEST; // Par défaut
  }
  
  uint8_t function = frame[1];
  
  // Pour les fonctions de lecture
  if (function == READ_HOLDING_REGISTERS || function == READ_INPUT_REGISTERS) {
    // Si la trame fait 8 bytes, c'est une requête
    if (frame.size() == 8) {
      return FRAME_REQUEST;
    }
    
    // Si le 3ème byte est un ByteCount raisonnable, c'est une réponse
    uint8_t third_byte = frame[2];
    if (third_byte > 0 && third_byte <= 250 && frame.size() == (size_t)(3 + third_byte + 2)) {
      return FRAME_RESPONSE;
    }
  }
  
  // Pour les fonctions d'écriture, requête et réponse ont la même structure
  // On considère par défaut que c'est une requête
  return FRAME_REQUEST;
}

void ModbusListenerHub::notify_text_sensors(const std::vector<uint8_t> &frame, FrameType type) {
#ifdef USE_TEXT_SENSOR
  std::string hex_string = format_hex(frame);
  
  for (auto *sensor : text_sensors_) {
    sensor->update_frame(hex_string, type);
  }
#endif
}

std::string ModbusListenerHub::format_hex(const std::vector<uint8_t> &data) {
  std::string result;
  
  // Ajouter les guillemets d'ouverture si nécessaire
  if (use_quote_) {
    result += "'";
  }
  
  // Ajouter le crochet d'ouverture si nécessaire
  if (use_bracket_) {
    result += "[";
  }
  
  for (size_t i = 0; i < data.size(); i++) {
    // Ajouter le préfixe 0x si nécessaire
    if (use_hexa_) {
      char buf[8];
      sprintf(buf, "0x%02X", data[i]);
      result += buf;
    } else {
      char buf[4];
      sprintf(buf, "%02X", data[i]);
      result += buf;
    }
    
    // Ajouter la virgule ou l'espace selon l'option
    if (i < data.size() - 1) {
      if (use_comma_) {
        result += ", ";
      } else {
        result += " ";
      }
    }
  }
  
  // Ajouter le crochet de fermeture si nécessaire
  if (use_bracket_) {
    result += "]";
  }
  
  // Ajouter les guillemets de fermeture si nécessaire
  if (use_quote_) {
    result += "'";
  }
  
  return result;
}

}  // namespace modbus_listener
}  // namespace esphome
