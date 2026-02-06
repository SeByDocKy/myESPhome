#include "rylr998.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace rylr998 {

static const char *const TAG = "rylr998";

void RYLR998Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RYLR998...");

  // Wait for module to be ready
  delay(500);

  // Test AT communication
  if (!this->send_command_("AT", 1000)) {
    ESP_LOGE(TAG, "Failed to communicate with RYLR998 module");
    this->mark_failed();
    return;
  }

  // Reset module
  ESP_LOGD(TAG, "Resetting module...");
  this->send_command_("AT+RESET", 2000);
  delay(1000);

  // Configure module with AT commands
  ESP_LOGD(TAG, "Configuring module...");

  // Set address
  char address_cmd[32];
  snprintf(address_cmd, sizeof(address_cmd), "AT+ADDRESS=%d", this->address_);
  if (!this->send_command_(address_cmd, 1000)) {
    ESP_LOGW(TAG, "Failed to set address");
  }
  delay(COMMAND_DELAY_MS);

  // Set frequency
  char freq_cmd[64];
  snprintf(freq_cmd, sizeof(freq_cmd), "AT+BAND=%lu,M", this->frequency_);
  if (!this->send_command_(freq_cmd, 1000)) {
    ESP_LOGW(TAG, "Failed to set frequency");
  }
  delay(COMMAND_DELAY_MS);

  // Set RF parameters
  uint8_t bw_code = this->bandwidth_to_code_(this->bandwidth_);
  char param_cmd[64];
  snprintf(param_cmd, sizeof(param_cmd), "AT+PARAMETER=%d,%d,%d,%d",
           this->spreading_factor_, bw_code, this->coding_rate_, this->preamble_length_);
  if (!this->send_command_(param_cmd, 1000)) {
    ESP_LOGW(TAG, "Failed to set RF parameters");
  }
  delay(COMMAND_DELAY_MS);

  // Set network ID
  char network_cmd[32];
  snprintf(network_cmd, sizeof(network_cmd), "AT+NETWORKID=%d", this->network_id_);
  if (!this->send_command_(network_cmd, 1000)) {
    ESP_LOGW(TAG, "Failed to set network ID");
  }
  delay(COMMAND_DELAY_MS);

  // Set TX power
  char power_cmd[32];
  snprintf(power_cmd, sizeof(power_cmd), "AT+CRFOP=%d", this->tx_power_);
  if (!this->send_command_(power_cmd, 1000)) {
    ESP_LOGW(TAG, "Failed to set TX power");
  }
  delay(COMMAND_DELAY_MS);

  this->initialized_ = true;
  ESP_LOGCONFIG(TAG, "RYLR998 setup complete");
}

void RYLR998Component::loop() {
  // Read available data from UART
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    
    if (c == '\n') {
      // Process complete line
      if (!this->rx_buffer_.empty()) {
        // Remove \r if present
        if (this->rx_buffer_.back() == '\r') {
          this->rx_buffer_.pop_back();
        }
        this->process_rx_line_(this->rx_buffer_);
        this->rx_buffer_.clear();
      }
    } else if (c != '\r') {
      this->rx_buffer_ += (char) c;
    }
  }
}

void RYLR998Component::dump_config() {
  ESP_LOGCONFIG(TAG, "RYLR998:");
  ESP_LOGCONFIG(TAG, "  Address: %d", this->address_);
  ESP_LOGCONFIG(TAG, "  Frequency: %lu Hz", this->frequency_);
  ESP_LOGCONFIG(TAG, "  Spreading Factor: %d", this->spreading_factor_);
  ESP_LOGCONFIG(TAG, "  Bandwidth: %s", this->bandwidth_to_string_(this->bandwidth_).c_str());
  ESP_LOGCONFIG(TAG, "  Coding Rate: 4/%d", this->coding_rate_ + 4);
  ESP_LOGCONFIG(TAG, "  Preamble Length: %d", this->preamble_length_);
  ESP_LOGCONFIG(TAG, "  Network ID: %d", this->network_id_);
  ESP_LOGCONFIG(TAG, "  TX Power: %d dBm", this->tx_power_);
  
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with RYLR998 failed!");
  }
}

bool RYLR998Component::send_command_(const std::string &command, uint32_t timeout_ms) {
  // Clear RX buffer
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
  }
  this->rx_buffer_.clear();

  // Send command with CR+LF
  std::string full_command = command + "\r\n";
  this->write_str(full_command.c_str());
  this->flush();

  ESP_LOGV(TAG, "Sent command: %s", command.c_str());

  // Wait for response
  return this->wait_for_response_("+OK", timeout_ms) || 
         this->wait_for_response_("+READY", timeout_ms);
}

bool RYLR998Component::wait_for_response_(const std::string &expected, uint32_t timeout_ms) {
  uint32_t start_time = millis();
  std::string response_buffer;

  while (millis() - start_time < timeout_ms) {
    while (this->available()) {
      uint8_t c;
      this->read_byte(&c);
      
      if (c == '\n') {
        // Check if response matches expected
        if (response_buffer.find(expected) != std::string::npos) {
          ESP_LOGV(TAG, "Received expected response: %s", response_buffer.c_str());
          return true;
        }
        response_buffer.clear();
      } else if (c != '\r') {
        response_buffer += (char) c;
      }
    }
    delay(10);
  }

  ESP_LOGW(TAG, "Timeout waiting for response: %s", expected.c_str());
  return false;
}

void RYLR998Component::process_rx_line_(const std::string &line) {
  ESP_LOGV(TAG, "RX: %s", line.c_str());

  // Check for received message: +RCV=<Address>,<Length>,<Data>,<RSSI>,<SNR>
  if (line.find("+RCV=") == 0) {
    // Parse the received message
    size_t pos = 5;  // Skip "+RCV="
    
    // Parse address
    size_t comma1 = line.find(',', pos);
    if (comma1 == std::string::npos) return;
    uint16_t address = std::stoi(line.substr(pos, comma1 - pos));
    
    // Parse length
    pos = comma1 + 1;
    size_t comma2 = line.find(',', pos);
    if (comma2 == std::string::npos) return;
    int length = std::stoi(line.substr(pos, comma2 - pos));
    
    // Parse data
    pos = comma2 + 1;
    size_t comma3 = line.find(',', pos);
    if (comma3 == std::string::npos) return;
    std::string data_str = line.substr(pos, comma3 - pos);
    
    // Parse RSSI
    pos = comma3 + 1;
    size_t comma4 = line.find(',', pos);
    if (comma4 == std::string::npos) return;
    int rssi = std::stoi(line.substr(pos, comma4 - pos));
    
    // Parse SNR
    pos = comma4 + 1;
    int snr = std::stoi(line.substr(pos));
    
    // Convert data string to bytes
    std::vector<uint8_t> data(data_str.begin(), data_str.end());
    
    ESP_LOGD(TAG, "Received message from %d: %s (RSSI: %d, SNR: %d)", 
             address, data_str.c_str(), rssi, snr);
    
    // Trigger callback
    this->packet_callback_.call(address, data, rssi, snr);
  }
}

bool RYLR998Component::send_data(uint16_t destination, const std::vector<uint8_t> &data) {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "Module not initialized");
    return false;
  }

  if (data.size() > 240) {
    ESP_LOGE(TAG, "Data too large (max 240 bytes)");
    return false;
  }

  // Convert data to ASCII string
  std::string data_str(data.begin(), data.end());
  
  // Build AT+SEND command
  char send_cmd[300];
  snprintf(send_cmd, sizeof(send_cmd), "AT+SEND=%d,%d,%s",
           destination, (int)data.size(), data_str.c_str());
  
  ESP_LOGD(TAG, "Sending to %d: %s", destination, data_str.c_str());
  
  return this->send_command_(send_cmd, 2000);
}

bool RYLR998Component::send_data(uint16_t destination, const std::string &data) {
  std::vector<uint8_t> data_bytes(data.begin(), data.end());
  return this->send_data(destination, data_bytes);
}

uint8_t RYLR998Component::bandwidth_to_code_(uint32_t bandwidth) {
  switch (bandwidth) {
    case 125000: return 7;  // 125 KHz
    case 250000: return 8;  // 250 KHz
    case 500000: return 9;  // 500 KHz
    default:
      ESP_LOGW(TAG, "Invalid bandwidth %lu, using 125 KHz", bandwidth);
      return 7;
  }
}

std::string RYLR998Component::bandwidth_to_string_(uint32_t bandwidth) {
  switch (bandwidth) {
    case 125000: return "125 KHz";
    case 250000: return "250 KHz";
    case 500000: return "500 KHz";
    default: return "Unknown";
  }
}

}  // namespace rylr998
}  // namespace esphome
