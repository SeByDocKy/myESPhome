#include "rylr998.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"

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

  // Configure module with AT commands
  ESP_LOGD(TAG, "Configuring module...");

  // Set address
  char address_cmd[32];
  snprintf(address_cmd, sizeof(address_cmd), "AT+ADDRESS=%d", this->address_);
  if (!this->send_command_(address_cmd, 1000)) {
    ESP_LOGW(TAG, "Failed to set address");
  }
  delay(COMMAND_DELAY_MS);

  // Set frequency - RYLR998 uses Hz directly, no suffix
  char freq_cmd[32];
  snprintf(freq_cmd, sizeof(freq_cmd), "AT+BAND=%lu", this->frequency_);
  if (!this->send_command_(freq_cmd, 1000)) {
    ESP_LOGW(TAG, "Failed to set frequency");
  }
  delay(COMMAND_DELAY_MS);

  // Set RF parameters: SF, BW, CR, Preamble
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

  // Synchronise le number avec la valeur initiale de tx_power configurée en YAML
  if (this->tx_power_number_ != nullptr) {
    this->tx_power_number_->publish_state(static_cast<float>(this->tx_power_));
  }
}

void RYLR998Component::loop() {
  // Read available data from UART
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);

    if (c == '\n') {
      // Process complete line - strip trailing \r if present
      if (!this->rx_buffer_.empty()) {
        if (this->rx_buffer_.back() == '\r') {
          this->rx_buffer_.pop_back();
        }
        if (!this->rx_buffer_.empty()) {
          this->process_rx_line_(this->rx_buffer_);
        }
        this->rx_buffer_.clear();
      }
    } else {
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

  ESP_LOGV(TAG, "TX: %s", command.c_str());

  // Wait for +OK or +READY in a single pass
  uint32_t start = millis();
  std::string line;

  while (millis() - start < timeout_ms) {
    while (this->available()) {
      uint8_t c;
      this->read_byte(&c);

      if (c == '\n') {
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') {
          line.pop_back();
        }
        ESP_LOGV(TAG, "RX cmd: %s", line.c_str());
        if (line.find("+OK") != std::string::npos ||
            line.find("+READY") != std::string::npos) {
          return true;
        }
        // Error responses
        if (line.find("+ERR") != std::string::npos) {
          ESP_LOGW(TAG, "Module returned error: %s", line.c_str());
          return false;
        }
        line.clear();
      } else {
        line += (char) c;
      }
    }
    delay(5);
  }

  ESP_LOGW(TAG, "Timeout waiting for +OK after: %s", command.c_str());
  return false;
}

// Helper to strip leading/trailing spaces from a string
static std::string trim_(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

void RYLR998Component::process_rx_line_(const std::string &line) {
  ESP_LOGV(TAG, "RX: '%s'", line.c_str());

  // Acknowledge responses from AT+SEND (non-blocking transmit)
  if (line.find("+OK") != std::string::npos) {
    ESP_LOGV(TAG, "TX acknowledged");
    return;
  }
  if (line.find("+ERR=17") != std::string::npos) {
    // ERR=17 = TX busy - LoRa air-time not finished yet, will retry next interval
    ESP_LOGD(TAG, "TX busy (ERR=17), dropped - PacketTransport will retry");
    return;
  }
  if (line.find("+ERR") != std::string::npos) {
    ESP_LOGW(TAG, "Module error: %s", line.c_str());
    return;
  }

  // Check for received message: +RCV=<Address>,<Length>,<Data>,<RSSI>,<SNR>
  // Note: RYLR998 may output spaces after commas: "+RCV=50, 5, HELLO, -99, 40"
  if (line.find("+RCV=") != 0) {
    return;
  }

  std::string payload = line.substr(5);  // skip "+RCV="

  // We need to find the last two commas (RSSI and SNR) counting from the end
  // because <Data> itself may contain commas.
  // Format: <Address>,<Length>,<Data>,<RSSI>,<SNR>

  // Find last comma -> SNR
  size_t snr_comma = payload.rfind(',');
  if (snr_comma == std::string::npos) return;

  // Find second-to-last comma -> RSSI
  size_t rssi_comma = payload.rfind(',', snr_comma - 1);
  if (rssi_comma == std::string::npos) return;

  // Find first comma -> Address end
  size_t addr_comma = payload.find(',');
  if (addr_comma == std::string::npos) return;

  // Find second comma -> Length end
  size_t len_comma = payload.find(',', addr_comma + 1);
  if (len_comma == std::string::npos) return;

  // Parse fields
  uint16_t address = (uint16_t) std::stoi(trim_(payload.substr(0, addr_comma)));
  // length field present but we use actual data size
  std::string data_str = trim_(payload.substr(len_comma + 1, rssi_comma - len_comma - 1));
  int rssi = std::stoi(trim_(payload.substr(rssi_comma + 1, snr_comma - rssi_comma - 1)));
  int snr  = std::stoi(trim_(payload.substr(snr_comma + 1)));

  // Data arrives as hex-ASCII (e.g. "48454C4C4F") - decode to bytes
  std::vector<uint8_t> data;
  if (data_str.size() % 2 == 0) {
    for (size_t i = 0; i < data_str.size(); i += 2) {
      uint8_t byte = (uint8_t)std::stoi(data_str.substr(i, 2), nullptr, 16);
      data.push_back(byte);
    }
  } else {
    // Odd length - treat as raw (fallback)
    data.assign(data_str.begin(), data_str.end());
  }

  ESP_LOGD(TAG, "RCV from %d: %d bytes (RSSI: %d, SNR: %d)", address, (int)data.size(), rssi, snr);

  float rssi_f = static_cast<float>(rssi);
  float snr_f  = static_cast<float>(snr);

  // Notify via plain C function pointer - NO virtual dispatch, NO vtable lookup
  // Completely immune to heap corruption that corrupts vtable pointers
  if (this->raw_cb_ != nullptr) {
    this->raw_cb_(data, rssi_f, snr_f);
  }

  // Fire automation trigger (only if set)
  if (this->packet_trigger_ != nullptr) {
    this->packet_trigger_->trigger(data, rssi_f, snr_f);
  }

  // Legacy callbacks
  this->packet_callback_.call(address, data, rssi, snr);

  // Publish RSSI / SNR sensors (optionnels — nullptr si non déclarés en YAML)
  if (this->rssi_sensor_ != nullptr) this->rssi_sensor_->publish_state(rssi_f);
  if (this->snr_sensor_  != nullptr) this->snr_sensor_->publish_state(snr_f);
}

bool RYLR998Component::transmit_packet(const std::vector<uint8_t> &data) {
  return this->transmit_packet(0, data);
}

bool RYLR998Component::transmit_packet(uint16_t destination, const std::vector<uint8_t> &data) {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "Module not initialized");
    return false;
  }
  if (data.size() > 240) {
    ESP_LOGE(TAG, "Data too large (max 240 bytes), got %d", (int) data.size());
    return false;
  }


  // RYLR998 requires hex-ASCII data in AT+SEND
  // Rate limit: LoRa air-time at SF9/BW125 is ~450ms. Drop if sent too recently.
  // PacketTransport will call us again in ~500ms anyway (it retransmits by design).
  uint32_t now = millis();
  if (now - this->last_tx_time_ < TX_MIN_INTERVAL_MS) {
    return true;  // silently drop, will be sent next call
  }
  this->last_tx_time_ = now;

  // Encode as hex-ASCII (stack buffer only, no heap allocation)
  char hex_buf[481];  // 240 bytes max -> 480 hex chars + null
  for (size_t i = 0; i < data.size(); i++) {
    snprintf(hex_buf + i * 2, 3, "%02X", data[i]);
  }
  hex_buf[data.size() * 2] = '\0';

  char send_cmd[520];
  snprintf(send_cmd, sizeof(send_cmd), "AT+SEND=%d,%d,%s",
           destination, (int)(data.size() * 2), hex_buf);

  ESP_LOGD(TAG, "TX to %d (%d bytes): %s", destination, (int)data.size(), hex_buf);
  this->send_raw_(send_cmd);
  return true;
}

void RYLR998Component::send_raw_(const std::string &command) {
  std::string full = command + "\r\n";
  this->write_str(full.c_str());
  this->flush();
}

bool RYLR998Component::send_data(uint16_t destination, const std::vector<uint8_t> &data) {
  return this->transmit_packet(destination, data);
}

bool RYLR998Component::send_data(uint16_t destination, const std::string &data) {
  return this->transmit_packet(destination, std::vector<uint8_t>(data.begin(), data.end()));
}

// ── Number TX power ───────────────────────────────────────────────────────────

void RYLR998Component::apply_tx_power(uint8_t power) {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "apply_tx_power: module not yet initialized, ignoring");
    return;
  }
  this->tx_power_ = power;

  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+CRFOP=%d", power);
  ESP_LOGD(TAG, "Setting TX power to %d dBm", power);

  if (!this->send_command_(cmd, 1000)) {
    ESP_LOGW(TAG, "Failed to set TX power to %d dBm", power);
    return;
  }
  ESP_LOGI(TAG, "TX power set to %d dBm", power);
}

uint8_t RYLR998Component::bandwidth_to_code_(uint32_t bandwidth) {
  switch (bandwidth) {
    case 125000: return 7;
    case 250000: return 8;
    case 500000: return 9;
    default:
      ESP_LOGW(TAG, "Unknown bandwidth %lu Hz, defaulting to 125 kHz", bandwidth);
      return 7;
  }
}

std::string RYLR998Component::bandwidth_to_string_(uint32_t bandwidth) {
  switch (bandwidth) {
    case 125000: return "125 KHz";
    case 250000: return "250 KHz";
    case 500000: return "500 KHz";
    default:     return "Unknown";
  }
}

}  // namespace rylr998
}  // namespace esphome
