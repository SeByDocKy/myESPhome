#include "rylr998.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace rylr998 {

static const char *const TAG = "rylr998";

// ── Sensor registration ───────────────────────────────────────────────────────
void RYLR998Component::set_rssi_sensor(esphome::sensor::Sensor *s) { rssi_sensor_ = s; }
void RYLR998Component::set_snr_sensor(esphome::sensor::Sensor *s)  { snr_sensor_  = s; }

// ── ESPHome lifecycle ─────────────────────────────────────────────────────────

void RYLR998Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RYLR998...");
  this->apply_config_();
}

void RYLR998Component::loop() {
  // Read all available bytes and accumulate in rx_buffer_
  while (this->available()) {
    char c = this->read();
    if (c == '\n') {
      // Remove trailing \r if present
      if (!rx_buffer_.empty() && rx_buffer_.back() == '\r') {
        rx_buffer_.pop_back();
      }
      if (!rx_buffer_.empty()) {
        this->process_line_(rx_buffer_);
        rx_buffer_.clear();
      }
    } else {
      rx_buffer_ += c;
    }
  }
}

void RYLR998Component::dump_config() {
  ESP_LOGCONFIG(TAG, "RYLR998:");
  ESP_LOGCONFIG(TAG, "  Address:      %u",  address_);
  ESP_LOGCONFIG(TAG, "  Network ID:   %u",  network_id_);
  ESP_LOGCONFIG(TAG, "  Band:         %u Hz", band_);
  ESP_LOGCONFIG(TAG, "  SF:           %u",  sf_);
  ESP_LOGCONFIG(TAG, "  BW:           %u",  bw_);
  ESP_LOGCONFIG(TAG, "  CR:           %u",  cr_);
  ESP_LOGCONFIG(TAG, "  Preamble:     %u",  preamble_);
  ESP_LOGCONFIG(TAG, "  Output power: %u dBm", output_power_);
  if (rssi_sensor_) LOG_SENSOR("  ", "RSSI sensor", rssi_sensor_);
  if (snr_sensor_)  LOG_SENSOR("  ", "SNR sensor",  snr_sensor_);
}

// ── Public TX API ─────────────────────────────────────────────────────────────

bool RYLR998Component::send(uint16_t dest_addr, const std::string &data) {
  std::string cmd = "AT+SEND=" + to_string(dest_addr) + ","
                  + to_string(data.size()) + "," + data;
  this->send_at_(cmd);
  return this->wait_for_ok_();
}

// ── Protected helpers ─────────────────────────────────────────────────────────

void RYLR998Component::send_at_(const std::string &cmd) {
  ESP_LOGD(TAG, "TX >> %s", cmd.c_str());
  this->write_str((cmd + "\r\n").c_str());
}

bool RYLR998Component::wait_for_ok_(uint32_t timeout_ms) {
  uint32_t start = millis();
  std::string buf;
  while (millis() - start < timeout_ms) {
    while (this->available()) {
      char c = this->read();
      if (c == '\n') {
        if (!buf.empty() && buf.back() == '\r') buf.pop_back();
        if (buf == "+OK") return true;
        // Could be a +RCV while waiting — process it anyway
        if (!buf.empty()) this->process_line_(buf);
        buf.clear();
      } else {
        buf += c;
      }
    }
    yield();
  }
  ESP_LOGW(TAG, "Timeout waiting for +OK");
  return false;
}

void RYLR998Component::apply_config_() {
  // Reset
  this->send_at_("AT+RESET");
  delay(500);  // Give the module time to reboot

  // Network ID
  this->send_at_("AT+NETWORKID=" + to_string(network_id_));
  this->wait_for_ok_();

  // Address
  this->send_at_("AT+ADDRESS=" + to_string(address_));
  this->wait_for_ok_();

  // RF parameters: SPREADING FACTOR, BW, CR, PREAMBLE
  this->send_at_("AT+PARAMETER=" + to_string(sf_) + ","
               + to_string(bw_) + "," + to_string(cr_) + ","
               + to_string(preamble_));
  this->wait_for_ok_();

  // Band (frequency)
  this->send_at_("AT+BAND=" + to_string(band_));
  this->wait_for_ok_();

  // AES password
  this->send_at_("AT+CPIN=" + password_);
  this->wait_for_ok_();

  // Output power
  this->send_at_("AT+CRFOP=" + to_string(output_power_));
  this->wait_for_ok_();

  ESP_LOGI(TAG, "RYLR998 configured — ready.");
}

// ── RX line parser ────────────────────────────────────────────────────────────
//
// The RYLR998 unsolicited receive format is:
//   +RCV=<Address>,<Length>,<Data>,<RSSI>,<SNR>
//
// Example:
//   +RCV=50,5,HELLO,-59,8
//
void RYLR998Component::process_line_(const std::string &line) {
  ESP_LOGD(TAG, "RX << %s", line.c_str());

  if (line.rfind("+RCV=", 0) != 0) {
    // Not a receive notification (could be +OK, +ERR, etc.)
    return;
  }

  // Strip the "+RCV=" prefix
  std::string payload = line.substr(5);

  // Tokenise by comma — we need at least 5 fields:
  // [0] address, [1] length, [2..N-2] data (may contain commas), [N-1] snr, [N-2] rssi
  // Strategy: find the last two commas for rssi and snr
  size_t last_comma   = payload.rfind(',');
  if (last_comma == std::string::npos) return;
  size_t second_last  = payload.rfind(',', last_comma - 1);
  if (second_last == std::string::npos) return;

  std::string snr_str  = payload.substr(last_comma + 1);
  std::string rssi_str = payload.substr(second_last + 1, last_comma - second_last - 1);

  // The data is everything between the second comma (after length) and second_last
  // But first split off address and length
  size_t first_comma  = payload.find(',');
  if (first_comma == std::string::npos) return;
  size_t second_comma = payload.find(',', first_comma + 1);
  if (second_comma == std::string::npos) return;

  // std::string src_addr = payload.substr(0, first_comma);
  // std::string length   = payload.substr(first_comma + 1, second_comma - first_comma - 1);
  std::string data     = payload.substr(second_comma + 1, second_last - second_comma - 1);

  float rssi = parse_number<float>(rssi_str).value_or(0.0f);
  float snr  = parse_number<float>(snr_str).value_or(0.0f);

  ESP_LOGI(TAG, "RCV data='%s' RSSI=%.0f dBm SNR=%.0f dB", data.c_str(), rssi, snr);

  if (rssi_sensor_ != nullptr) rssi_sensor_->publish_state(rssi);
  if (snr_sensor_  != nullptr) snr_sensor_->publish_state(snr);
}

}  // namespace rylr998
}  // namespace esphome
