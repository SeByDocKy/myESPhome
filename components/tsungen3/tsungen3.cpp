#include "tsungen3.h"
#include "esphome/core/log.h"

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <cstring>

namespace esphome {
namespace tsungen3 {

static const char *const TAG = "tsungen3";
static const uint32_t SOCKET_TIMEOUT_MS = 3000;

// ---------------------------------------------------------------------------
// CRC helpers
// ---------------------------------------------------------------------------

// Standard Modbus RTU CRC16 (poly 0xA001, init 0xFFFF)
uint16_t TSunGen3Component::modbus_crc16_(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t pos = 0; pos < len; pos++) {
    crc ^= (uint16_t) data[pos];
    for (uint8_t i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

// Solarman V5 checksum: sum of all bytes between Start and Checksum/End, mod 256
uint8_t TSunGen3Component::v5_checksum_(const uint8_t *data, size_t len) {
  uint32_t sum = 0;
  for (size_t i = 0; i < len; i++)
    sum += data[i];
  return (uint8_t) (sum & 0xFF);
}

// ---------------------------------------------------------------------------
// Register decoding helpers
// ---------------------------------------------------------------------------

// NOTE: individual 16-bit Modbus registers are standard Modbus big-endian on the
// wire. For 32-bit values spanning two consecutive registers, the low word is at
// the lower register address ("...LE" naming in the TSUN proxy's register table)
// -- this combination has not been independently verified against a live capture,
// only against the s-allius/tsun-gen3-proxy wiki documentation.
uint16_t TSunGen3Component::get_u16_(const std::vector<uint8_t> &regs, uint16_t reg, uint16_t start_reg) {
  size_t index = (size_t) (reg - start_reg) * 2;
  if (index + 1 >= regs.size())
    return 0;
  return ((uint16_t) regs[index] << 8) | (uint16_t) regs[index + 1];
}

uint32_t TSunGen3Component::get_u32_(const std::vector<uint8_t> &regs, uint16_t reg, uint16_t start_reg) {
  uint16_t low = get_u16_(regs, reg, start_reg);
  uint16_t high = get_u16_(regs, reg + 1, start_reg);
  return ((uint32_t) high << 16) | (uint32_t) low;
}

// ---------------------------------------------------------------------------
// Frame construction / parsing
// ---------------------------------------------------------------------------

std::vector<uint8_t> TSunGen3Component::build_read_request_(uint16_t start_reg, uint16_t count) {
  // Modbus RTU request: address(1) + function(1) + start(2 BE) + count(2 BE) + crc(2 LE)
  std::vector<uint8_t> modbus;
  modbus.push_back(this->modbus_address_);
  modbus.push_back(MB_READ_HOLDING_REGISTERS);
  modbus.push_back((start_reg >> 8) & 0xFF);
  modbus.push_back(start_reg & 0xFF);
  modbus.push_back((count >> 8) & 0xFF);
  modbus.push_back(count & 0xFF);
  uint16_t crc = modbus_crc16_(modbus.data(), modbus.size());
  modbus.push_back(crc & 0xFF);         // CRC low byte first
  modbus.push_back((crc >> 8) & 0xFF);

  // Solarman V5 request payload: 15 bytes + modbus frame
  std::vector<uint8_t> payload;
  payload.push_back(0x02);              // Frame Type: solar inverter
  payload.push_back(0x00);              // Sensor Type LE
  payload.push_back(0x00);
  payload.insert(payload.end(), {0x00, 0x00, 0x00, 0x00});  // Total Working Time
  payload.insert(payload.end(), {0x00, 0x00, 0x00, 0x00});  // Power On Time
  payload.insert(payload.end(), {0x00, 0x00, 0x00, 0x00});  // Offset Time
  payload.insert(payload.end(), modbus.begin(), modbus.end());

  std::vector<uint8_t> frame;
  frame.push_back(V5_START);
  uint16_t len = (uint16_t) payload.size();
  frame.push_back(len & 0xFF);
  frame.push_back((len >> 8) & 0xFF);
  frame.push_back(V5_CTRL_REQUEST & 0xFF);
  frame.push_back((V5_CTRL_REQUEST >> 8) & 0xFF);

  this->v5_serial_++;
  frame.push_back(this->v5_serial_);
  frame.push_back(0x00);

  frame.push_back(this->logger_serial_ & 0xFF);
  frame.push_back((this->logger_serial_ >> 8) & 0xFF);
  frame.push_back((this->logger_serial_ >> 16) & 0xFF);
  frame.push_back((this->logger_serial_ >> 24) & 0xFF);

  frame.insert(frame.end(), payload.begin(), payload.end());

  uint8_t checksum = v5_checksum_(frame.data() + 1, frame.size() - 1);
  frame.push_back(checksum);
  frame.push_back(V5_END);

  return frame;
}

bool TSunGen3Component::parse_response_(const std::vector<uint8_t> &frame, std::vector<uint8_t> &register_data) {
  if (frame.size() < 13) {
    ESP_LOGW(TAG, "Response too short (%d bytes)", (int) frame.size());
    return false;
  }
  if (frame.front() != V5_START || frame.back() != V5_END) {
    ESP_LOGW(TAG, "Invalid start/end byte");
    return false;
  }

  uint16_t payload_len = (uint16_t) frame[1] | ((uint16_t) frame[2] << 8);
  size_t expected_total = 11 + payload_len + 2;
  if (frame.size() != expected_total) {
    ESP_LOGW(TAG, "Unexpected frame length: got %d, expected %d", (int) frame.size(), (int) expected_total);
    return false;
  }

  uint8_t checksum = v5_checksum_(frame.data() + 1, frame.size() - 3);
  if (checksum != frame[frame.size() - 2]) {
    ESP_LOGW(TAG, "V5 checksum mismatch");
    return false;
  }

  uint16_t ctrl = (uint16_t) frame[3] | ((uint16_t) frame[4] << 8);
  if (ctrl != V5_CTRL_RESPONSE) {
    ESP_LOGW(TAG, "Unexpected control code: 0x%04X", ctrl);
    return false;
  }

  if (payload_len < 14) {
    ESP_LOGW(TAG, "Response payload too short for Modbus frame");
    return false;
  }

  // Response payload: FrameType(1) + Status(1) + TotalWorkingTime(4) + PowerOnTime(4)
  //                    + OffsetTime(4) + Modbus RTU frame(variable)
  const uint8_t *modbus = frame.data() + 11 + 14;
  size_t modbus_len = payload_len - 14;

  if (modbus_len < 5) {
    ESP_LOGW(TAG, "Modbus frame too short");
    return false;
  }

  uint16_t mb_crc_calc = modbus_crc16_(modbus, modbus_len - 2);
  uint16_t mb_crc_recv = (uint16_t) modbus[modbus_len - 2] | ((uint16_t) modbus[modbus_len - 1] << 8);
  if (mb_crc_calc != mb_crc_recv) {
    ESP_LOGW(TAG, "Modbus CRC mismatch (calc 0x%04X, recv 0x%04X)", mb_crc_calc, mb_crc_recv);
    return false;
  }

  if (modbus[0] != this->modbus_address_) {
    ESP_LOGW(TAG, "Unexpected Modbus address 0x%02X", modbus[0]);
    return false;
  }
  if (modbus[1] != MB_READ_HOLDING_REGISTERS) {
    if ((modbus[1] & 0x80) != 0) {
      ESP_LOGW(TAG, "Modbus exception response, code 0x%02X", modbus_len > 2 ? modbus[2] : 0);
    } else {
      ESP_LOGW(TAG, "Unexpected Modbus function 0x%02X", modbus[1]);
    }
    return false;
  }

  uint8_t byte_count = modbus[2];
  if (modbus_len < (size_t) (3 + byte_count + 2)) {
    ESP_LOGW(TAG, "Truncated Modbus register data");
    return false;
  }

  register_data.assign(modbus + 3, modbus + 3 + byte_count);
  return true;
}

// ---------------------------------------------------------------------------
// TCP transaction (blocking, one connection per poll cycle)
// ---------------------------------------------------------------------------

bool TSunGen3Component::connect_and_transact_(const std::vector<uint8_t> &request, std::vector<uint8_t> &response) {
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;

  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%u", this->port_);

  if (::getaddrinfo(this->host_.c_str(), port_str, &hints, &res) != 0 || res == nullptr) {
    ESP_LOGW(TAG, "DNS/address resolution failed for %s", this->host_.c_str());
    return false;
  }

  int sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sock < 0) {
    ESP_LOGW(TAG, "Failed to create socket");
    ::freeaddrinfo(res);
    return false;
  }

  struct timeval tv{};
  tv.tv_sec = SOCKET_TIMEOUT_MS / 1000;
  tv.tv_usec = (SOCKET_TIMEOUT_MS % 1000) * 1000;
  ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  bool ok = true;
  if (::connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
    ESP_LOGW(TAG, "Connect to %s:%u failed", this->host_.c_str(), this->port_);
    ok = false;
  }
  ::freeaddrinfo(res);

  if (ok) {
    ssize_t sent = ::send(sock, request.data(), request.size(), 0);
    if (sent != (ssize_t) request.size()) {
      ESP_LOGW(TAG, "Short write to inverter (%d/%d bytes)", (int) sent, (int) request.size());
      ok = false;
    }
  }

  if (ok) {
    uint8_t buf[256];
    response.clear();
    uint32_t start = millis();
    while (millis() - start < SOCKET_TIMEOUT_MS) {
      ssize_t n = ::recv(sock, buf, sizeof(buf), 0);
      if (n > 0) {
        response.insert(response.end(), buf, buf + n);
        // Stop once we have a plausible full frame: start + length header present
        // and the buffer already reaches the length implied by that header.
        if (response.size() >= 3) {
          uint16_t payload_len = (uint16_t) response[1] | ((uint16_t) response[2] << 8);
          size_t expected_total = 11 + payload_len + 2;
          if (response.size() >= expected_total)
            break;
        }
      } else if (n == 0) {
        break;  // peer closed
      } else {
        break;  // timeout or error
      }
    }
    if (response.empty()) {
      ESP_LOGW(TAG, "No response from inverter");
      ok = false;
    }
  }

  ::close(sock);
  return ok;
}

// ---------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------

void TSunGen3Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TSUN GEN3 PLUS component...");
}

void TSunGen3Component::update() {
  std::vector<uint8_t> request = this->build_read_request_(REG_BLOCK_START, REG_BLOCK_COUNT);
  std::vector<uint8_t> response;

  if (!this->connect_and_transact_(request, response)) {
    ESP_LOGW(TAG, "Poll of %s:%u failed", this->host_.c_str(), this->port_);
    return;
  }

  std::vector<uint8_t> register_data;
  if (!this->parse_response_(response, register_data)) {
    ESP_LOGW(TAG, "Failed to parse response from %s:%u", this->host_.c_str(), this->port_);
    return;
  }

  if (register_data.size() != (size_t) REG_BLOCK_COUNT * 2) {
    ESP_LOGW(TAG, "Unexpected register payload size: %d bytes", (int) register_data.size());
    return;
  }

  this->handle_live_block_(register_data, REG_BLOCK_START, REG_BLOCK_COUNT);
}

void TSunGen3Component::handle_live_block_(const std::vector<uint8_t> &regs, uint16_t start_reg, uint16_t count) {
#ifdef USE_TEXT_SENSOR
  char hex[8];
  if (this->inverter_status_text_sensor_ != nullptr) {
    snprintf(hex, sizeof(hex), "0x%04X", get_u16_(regs, 0x3000, start_reg));
    this->inverter_status_text_sensor_->publish_state(hex);
  }
  if (this->event_alarms_text_sensor_ != nullptr) {
    snprintf(hex, sizeof(hex), "0x%04X", get_u16_(regs, 0x3003, start_reg));
    this->event_alarms_text_sensor_->publish_state(hex);
  }
  if (this->event_faults_text_sensor_ != nullptr) {
    snprintf(hex, sizeof(hex), "0x%04X", get_u16_(regs, 0x3004, start_reg));
    this->event_faults_text_sensor_->publish_state(hex);
  }
#endif

#ifdef USE_SENSOR
  if (this->grid_voltage_sensor_ != nullptr)
    this->grid_voltage_sensor_->publish_state(get_u16_(regs, 0x3009, start_reg) * 0.1f);
  if (this->grid_current_sensor_ != nullptr)
    this->grid_current_sensor_->publish_state(get_u16_(regs, 0x300a, start_reg) * 0.01f);
  if (this->grid_frequency_sensor_ != nullptr)
    this->grid_frequency_sensor_->publish_state(get_u16_(regs, 0x300b, start_reg) * 0.01f);
  if (this->temperature_sensor_ != nullptr)
    // Register stores (actual_temperature + 40)
    this->temperature_sensor_->publish_state((float) get_u16_(regs, 0x300c, start_reg) - 40.0f);
  if (this->rated_power_sensor_ != nullptr)
    this->rated_power_sensor_->publish_state((float) get_u16_(regs, 0x300e, start_reg));
  if (this->current_power_sensor_ != nullptr)
    this->current_power_sensor_->publish_state(get_u16_(regs, 0x300f, start_reg) * 0.1f);

  static const uint16_t PV_BASE[4] = {0x3010, 0x3013, 0x3016, 0x3019};
  for (uint8_t i = 0; i < 4; i++) {
    if (this->pv_voltage_sensor_[i] != nullptr)
      this->pv_voltage_sensor_[i]->publish_state(get_u16_(regs, PV_BASE[i], start_reg) * 0.1f);
    if (this->pv_current_sensor_[i] != nullptr)
      this->pv_current_sensor_[i]->publish_state(get_u16_(regs, PV_BASE[i] + 1, start_reg) * 0.01f);
    if (this->pv_power_sensor_[i] != nullptr)
      this->pv_power_sensor_[i]->publish_state(get_u16_(regs, PV_BASE[i] + 2, start_reg) * 0.1f);
  }

  if (this->ac_daily_energy_sensor_ != nullptr)
    this->ac_daily_energy_sensor_->publish_state(get_u16_(regs, 0x301c, start_reg) * 0.01f);
  if (this->ac_total_energy_sensor_ != nullptr)
    this->ac_total_energy_sensor_->publish_state(get_u32_(regs, 0x301d, start_reg) * 0.01f);
#endif
}

void TSunGen3Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TSUN GEN3 PLUS:");
  ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Modbus address: %u", this->modbus_address_);
  ESP_LOGCONFIG(TAG, "  Logger serial: %u", (unsigned) this->logger_serial_);
}

}  // namespace tsungen3
}  // namespace esphome
