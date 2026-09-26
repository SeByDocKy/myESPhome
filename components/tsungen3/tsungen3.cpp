#include "tsungen3.h"
#include "esphome/core/log.h"

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <cmath>
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

// Wraps `tail` (a Modbus RTU frame, or AT-command bytes) into a full Solarman
// V5 request frame: Start + Length + ControlCode(request) + Serial + LoggerSerial
// + [FrameType + SensorType + 3x4 zero fields + tail] + Checksum + End.
std::vector<uint8_t> TSunGen3Component::wrap_v5_request_(uint8_t frame_type, uint16_t sensor_type,
                                                           const std::vector<uint8_t> &tail) {
  std::vector<uint8_t> payload;
  payload.push_back(frame_type);
  payload.push_back(sensor_type & 0xFF);
  payload.push_back((sensor_type >> 8) & 0xFF);
  payload.insert(payload.end(), {0x00, 0x00, 0x00, 0x00});  // Total Working Time
  payload.insert(payload.end(), {0x00, 0x00, 0x00, 0x00});  // Power On Time
  payload.insert(payload.end(), {0x00, 0x00, 0x00, 0x00});  // Offset Time
  payload.insert(payload.end(), tail.begin(), tail.end());

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

// Validates the Solarman V5 envelope (start/end/length/checksum/control code)
// and returns the raw response payload (FrameType..tail, `payload_len` bytes).
bool TSunGen3Component::extract_v5_payload_(const std::vector<uint8_t> &frame, std::vector<uint8_t> &payload) {
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
    ESP_LOGW(TAG, "Response payload too short");
    return false;
  }

  payload.assign(frame.begin() + 11, frame.begin() + 11 + payload_len);
  return true;
}

// ---------------------------------------------------------------------------
// Read (function 0x03)
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
  modbus.push_back(crc & 0xFF);  // CRC low byte first
  modbus.push_back((crc >> 8) & 0xFF);

  return this->wrap_v5_request_(V5_FRAME_TYPE_INVERTER, V5_SENSOR_TYPE_MODBUS, modbus);
}

bool TSunGen3Component::parse_response_(const std::vector<uint8_t> &frame, std::vector<uint8_t> &register_data) {
  std::vector<uint8_t> payload;
  if (!this->extract_v5_payload_(frame, payload))
    return false;

  // Response payload: FrameType(1) + Status(1) + TotalWorkingTime(4) + PowerOnTime(4)
  //                    + OffsetTime(4) + Modbus RTU frame(variable)
  const uint8_t *modbus = payload.data() + 14;
  size_t modbus_len = payload.size() - 14;

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
// Write single register (function 0x06)
// ---------------------------------------------------------------------------

std::vector<uint8_t> TSunGen3Component::build_write_request_(uint16_t reg, uint16_t value) {
  // Modbus RTU request: address(1) + function(1) + reg(2 BE) + value(2 BE) + crc(2 LE)
  std::vector<uint8_t> modbus;
  modbus.push_back(this->modbus_address_);
  modbus.push_back(MB_WRITE_SINGLE_REGISTER);
  modbus.push_back((reg >> 8) & 0xFF);
  modbus.push_back(reg & 0xFF);
  modbus.push_back((value >> 8) & 0xFF);
  modbus.push_back(value & 0xFF);
  uint16_t crc = modbus_crc16_(modbus.data(), modbus.size());
  modbus.push_back(crc & 0xFF);
  modbus.push_back((crc >> 8) & 0xFF);

  return this->wrap_v5_request_(V5_FRAME_TYPE_INVERTER, V5_SENSOR_TYPE_MODBUS, modbus);
}

// Per Modbus spec, a function-0x06 response echoes address+function+reg+value
// verbatim -- this checks that echo matches what was requested.
bool TSunGen3Component::parse_write_response_(const std::vector<uint8_t> &frame, uint16_t expected_reg,
                                               uint16_t expected_value) {
  std::vector<uint8_t> payload;
  if (!this->extract_v5_payload_(frame, payload))
    return false;

  const uint8_t *modbus = payload.data() + 14;
  size_t modbus_len = payload.size() - 14;

  if (modbus_len < 8) {
    ESP_LOGW(TAG, "Write response too short");
    return false;
  }

  uint16_t mb_crc_calc = modbus_crc16_(modbus, 6);
  uint16_t mb_crc_recv = (uint16_t) modbus[6] | ((uint16_t) modbus[7] << 8);
  if (mb_crc_calc != mb_crc_recv) {
    ESP_LOGW(TAG, "Write response CRC mismatch");
    return false;
  }

  if (modbus[0] != this->modbus_address_) {
    ESP_LOGW(TAG, "Unexpected Modbus address 0x%02X", modbus[0]);
    return false;
  }
  if (modbus[1] != MB_WRITE_SINGLE_REGISTER) {
    if ((modbus[1] & 0x80) != 0) {
      ESP_LOGW(TAG, "Write rejected: Modbus exception code 0x%02X", modbus[2]);
    } else {
      ESP_LOGW(TAG, "Unexpected Modbus function 0x%02X in write response", modbus[1]);
    }
    return false;
  }

  uint16_t reg_echo = ((uint16_t) modbus[2] << 8) | modbus[3];
  uint16_t val_echo = ((uint16_t) modbus[4] << 8) | modbus[5];
  if (reg_echo != expected_reg || val_echo != expected_value) {
    ESP_LOGW(TAG, "Write echo mismatch: reg 0x%04X (expected 0x%04X), value %u (expected %u)", reg_echo,
             expected_reg, val_echo, expected_value);
    return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// AT+ commands
// ---------------------------------------------------------------------------

std::vector<uint8_t> TSunGen3Component::build_at_command_request_(const std::string &cmd) {
  // AT-command payload tail: command text followed by a '\r' terminator.
  // Framing (Frame Type 0x01, Sensor Type 0x0002) confirmed against
  // s-allius/tsun-gen3-proxy's gen3plus/solarman_v5.py (send_at_cmd()).
  std::vector<uint8_t> tail(cmd.begin(), cmd.end());
  tail.push_back('\r');
  return this->wrap_v5_request_(V5_FRAME_TYPE_AT_CMD, V5_SENSOR_TYPE_AT_CMD, tail);
}

bool TSunGen3Component::parse_at_response_(const std::vector<uint8_t> &frame, std::string &text_out) {
  std::vector<uint8_t> payload;
  if (!this->extract_v5_payload_(frame, payload))
    return false;

  uint8_t ftype = payload[0];
  if (ftype != V5_FRAME_TYPE_AT_CMD && ftype != V5_FRAME_TYPE_AT_CMD_RSP) {
    ESP_LOGW(TAG, "Unexpected frame type in AT response: 0x%02X", ftype);
    return false;
  }

  text_out.assign(payload.begin() + 14, payload.end());
  // Strip a trailing CR/LF/NUL, if present.
  while (!text_out.empty() &&
         (text_out.back() == '\r' || text_out.back() == '\n' || text_out.back() == '\0')) {
    text_out.pop_back();
  }
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
  if (this->logger_serial_ == 0) {
    ESP_LOGW(TAG,
             "logger_serial is 0 (default) -- confirmed on real hardware to get NO "
             "response in client_mode. Set it to the inverter's real 'Monitoring SN' "
             "(printed on its sticker) if polling fails.");
  }

  // Small queues: only ever one poll in flight, plus at most one control
  // action (write/reset) queued up behind it. Sized generously (4) so a
  // button press during a poll isn't dropped.
  this->job_queue_ = xQueueCreate(4, sizeof(TSunGen3Job));
  this->result_queue_ = xQueueCreate(4, sizeof(TSunGen3JobResult *));
  if (this->job_queue_ == nullptr || this->result_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create job/result queues");
    this->mark_failed();
    return;
  }

  // All blocking network I/O happens on this task -- update()/set_power_percent()/
  // send_reset_command() only ever enqueue a job onto job_queue_, keeping the
  // main (cooperative) loop free.
  BaseType_t ok = xTaskCreate(&TSunGen3Component::task_trampoline_, "tsungen3", 8192, this, 5, &this->task_handle_);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "Failed to create background task");
    this->mark_failed();
  }
}

void TSunGen3Component::update() {
  TSunGen3Job job{JobType::POLL_READ};
  if (xQueueSend(this->job_queue_, &job, 0) != pdTRUE) {
    ESP_LOGW(TAG, "Background task busy, skipping this poll cycle");
  }
}

void TSunGen3Component::loop() {
  TSunGen3JobResult *result = nullptr;
  // Non-blocking drain: at most a few pointer-sized items per call, this is
  // effectively free on the main thread.
  while (xQueueReceive(this->result_queue_, &result, 0) == pdTRUE) {
    if (result != nullptr) {
      // TEMPORARY instrumentation to pin down a residual loop_time spike
      // after moving I/O to the background task -- remove once confirmed.
      uint32_t t0 = millis();
      this->process_result_(result);
      uint32_t dt = millis() - t0;
      if (dt > 5) {
        ESP_LOGW(TAG, "process_result_ (main thread) took %u ms for job type %d", (unsigned) dt, (int) result->type);
      }
      delete result;
    }
  }
}

void TSunGen3Component::task_trampoline_(void *param) {
  static_cast<TSunGen3Component *>(param)->run_task_();
}

void TSunGen3Component::run_task_() {
  TSunGen3Job job{};
  for (;;) {
    if (xQueueReceive(this->job_queue_, &job, portMAX_DELAY) != pdTRUE)
      continue;

    // TEMPORARY instrumentation (see loop()) -- measures the background
    // task's own transaction time, which should NOT affect loop_time at all
    // since it runs off the main thread. Confirms the split is doing its job.
    uint32_t task_t0 = millis();

    auto *result = new TSunGen3JobResult();
    result->type = job.type;

    switch (job.type) {
      case JobType::POLL_READ: {
        std::vector<uint8_t> request = this->build_read_request_(REG_BLOCK_START, REG_BLOCK_COUNT);
        std::vector<uint8_t> response;
        if (this->connect_and_transact_(request, response)) {
          std::vector<uint8_t> register_data;
          if (this->parse_response_(response, register_data) &&
              register_data.size() == (size_t) REG_BLOCK_COUNT * 2) {
            result->success = true;
            result->register_data = std::move(register_data);
          }
        }
        break;
      }
      case JobType::WRITE_POWER_PERCENT: {
        result->reg_value = job.reg_value;
        std::vector<uint8_t> request = this->build_write_request_(REG_OUTPUT_COEFFICIENT, job.reg_value);
        std::vector<uint8_t> response;
        if (this->connect_and_transact_(request, response) &&
            this->parse_write_response_(response, REG_OUTPUT_COEFFICIENT, job.reg_value)) {
          result->success = true;
        }
        break;
      }
      case JobType::RESET_AT_CMD: {
        std::vector<uint8_t> request = this->build_at_command_request_("AT+Z");
        std::vector<uint8_t> response;
        std::string text;
        if (this->connect_and_transact_(request, response) && this->parse_at_response_(response, text)) {
          result->success = true;
          result->text = std::move(text);
        }
        break;
      }
    }

    ESP_LOGD(TAG, "Background transaction (job type %d) took %u ms", (int) job.type,
             (unsigned) (millis() - task_t0));

    if (xQueueSend(this->result_queue_, &result, 0) != pdTRUE) {
      // Main loop fell behind and the result queue is full: drop it rather
      // than block the network task, and avoid leaking the heap allocation.
      delete result;
    }
  }
}

void TSunGen3Component::process_result_(TSunGen3JobResult *result) {
  switch (result->type) {
    case JobType::POLL_READ: {
      if (!result->success) {
        ESP_LOGW(TAG, "Poll of %s:%u failed", this->host_.c_str(), this->port_);
        break;
      }
      this->handle_live_block_(result->register_data, REG_BLOCK_START, REG_BLOCK_COUNT);
      break;
    }
    case JobType::WRITE_POWER_PERCENT: {
      float percent = result->reg_value * 100.0f / 1024.0f;
      if (!result->success) {
        ESP_LOGW(TAG, "Failed to write power_percent (%.1f%%) to %s:%u", percent, this->host_.c_str(), this->port_);
        break;
      }
      ESP_LOGI(TAG, "power_percent set to %.1f%% (register 0x%04X = %u)", percent, REG_OUTPUT_COEFFICIENT,
               result->reg_value);
      break;
    }
    case JobType::RESET_AT_CMD: {
      if (!result->success) {
        ESP_LOGW(TAG, "Failed to send AT+Z (reset) to %s:%u", this->host_.c_str(), this->port_);
        break;
      }
      ESP_LOGI(TAG, "AT+Z (reset) sent, inverter replied: %s", result->text.c_str());
      break;
    }
  }
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

  if (this->ac_energy_today_sensor_ != nullptr)
    this->ac_energy_today_sensor_->publish_state(get_u16_(regs, 0x301c, start_reg) * 0.01f);
  if (this->ac_energy_total_sensor_ != nullptr)
    this->ac_energy_total_sensor_->publish_state(get_u32_(regs, 0x301d, start_reg) * 0.01f);
#endif
}

void TSunGen3Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TSUN GEN3 PLUS:");
  ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Modbus address: %u", this->modbus_address_);
  ESP_LOGCONFIG(TAG, "  Logger serial: %u", (unsigned) this->logger_serial_);
}

// ---------------------------------------------------------------------------
// Control actions (number / output / button)
// ---------------------------------------------------------------------------

void TSunGen3Component::set_power_percent(float percent) {
  if (percent < 0.0f)
    percent = 0.0f;
  if (percent > 100.0f)
    percent = 100.0f;

  // Ratio 100/1024 (see REG_OUTPUT_COEFFICIENT) => register = percent * 1024 / 100
  uint16_t reg_value = (uint16_t) lroundf(percent * 1024.0f / 100.0f);
  if (reg_value > 1024)
    reg_value = 1024;

  TSunGen3Job job{JobType::WRITE_POWER_PERCENT, reg_value};
  if (xQueueSend(this->job_queue_, &job, 0) != pdTRUE) {
    ESP_LOGW(TAG, "Background task busy, dropped power_percent write (%.1f%%)", percent);
  }
}

void TSunGen3Component::send_reset_command() {
  TSunGen3Job job{JobType::RESET_AT_CMD};
  if (xQueueSend(this->job_queue_, &job, 0) != pdTRUE) {
    ESP_LOGW(TAG, "Background task busy, dropped AT+Z (reset) request");
  }
}

}  // namespace tsungen3
}  // namespace esphome
