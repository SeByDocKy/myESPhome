#include "deyegen3.h"
#include "esphome/core/log.h"

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <cmath>
#include <cstring>

namespace esphome {
namespace deyegen3 {

static const char *const TAG = "deyegen3";
static const uint32_t SOCKET_TIMEOUT_MS = 3000;

// ---------------------------------------------------------------------------
// CRC / checksum helpers -- identical to tsungen3 (generic Modbus/V5 math,
// no brand-specific behavior here).
// ---------------------------------------------------------------------------

uint16_t DeyeGen3Component::modbus_crc16_(const uint8_t *data, size_t len) {
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

uint8_t DeyeGen3Component::v5_checksum_(const uint8_t *data, size_t len) {
  uint32_t sum = 0;
  for (size_t i = 0; i < len; i++)
    sum += data[i];
  return (uint8_t) (sum & 0xFF);
}

// ---------------------------------------------------------------------------
// Register decoding helpers
// ---------------------------------------------------------------------------

uint16_t DeyeGen3Component::get_u16_(const std::vector<uint8_t> &regs, uint16_t reg, uint16_t start_reg) {
  size_t index = (size_t) (reg - start_reg) * 2;
  if (index + 1 >= regs.size())
    return 0;
  return ((uint16_t) regs[index] << 8) | (uint16_t) regs[index + 1];
}

int16_t DeyeGen3Component::get_s16_(const std::vector<uint8_t> &regs, uint16_t reg, uint16_t start_reg) {
  return (int16_t) get_u16_(regs, reg, start_reg);
}

// UNVERIFIED (no real hardware at time of writing): assumes the first
// register in a "rule 3" pair (as listed in the source project's YAML,
// e.g. Total Production = [0x003F, 0x0040]) is the HIGH word -- the
// opposite convention from tsungen3's TSUN registers. If total energy
// readings come back wildly wrong (e.g. jumping by 65536x), swap this to
// low-word-first to match tsungen3's get_u32_ instead.
uint32_t DeyeGen3Component::get_u32_hi_first_(const std::vector<uint8_t> &regs, uint16_t reg_hi, uint16_t reg_lo,
                                               uint16_t start_reg) {
  uint16_t high = get_u16_(regs, reg_hi, start_reg);
  uint16_t low = get_u16_(regs, reg_lo, start_reg);
  return ((uint32_t) high << 16) | (uint32_t) low;
}

const char *DeyeGen3Component::running_status_name_(uint16_t value) {
  switch (value) {
    case 0:
      return "Stand-by";
    case 1:
      return "Self-check";
    case 2:
      return "Normal";
    case 3:
      return "Warning";
    case 4:
      return "Fault";
    default:
      return nullptr;
  }
}

// ---------------------------------------------------------------------------
// Frame construction / parsing -- identical structure to tsungen3 (same V5
// envelope, same Modbus RTU framing); only the register map differs.
// ---------------------------------------------------------------------------

std::vector<uint8_t> DeyeGen3Component::wrap_v5_request_(uint8_t frame_type, uint16_t sensor_type,
                                                           const std::vector<uint8_t> &tail) {
  std::vector<uint8_t> payload;
  payload.push_back(frame_type);
  payload.push_back(sensor_type & 0xFF);
  payload.push_back((sensor_type >> 8) & 0xFF);
  payload.insert(payload.end(), {0x00, 0x00, 0x00, 0x00});
  payload.insert(payload.end(), {0x00, 0x00, 0x00, 0x00});
  payload.insert(payload.end(), {0x00, 0x00, 0x00, 0x00});
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

bool DeyeGen3Component::extract_v5_payload_(const std::vector<uint8_t> &frame, std::vector<uint8_t> &payload) {
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

std::vector<uint8_t> DeyeGen3Component::build_read_request_(uint16_t start_reg, uint16_t count) {
  std::vector<uint8_t> modbus;
  modbus.push_back(this->modbus_address_);
  modbus.push_back(MB_READ_HOLDING_REGISTERS);
  modbus.push_back((start_reg >> 8) & 0xFF);
  modbus.push_back(start_reg & 0xFF);
  modbus.push_back((count >> 8) & 0xFF);
  modbus.push_back(count & 0xFF);
  uint16_t crc = modbus_crc16_(modbus.data(), modbus.size());
  modbus.push_back(crc & 0xFF);
  modbus.push_back((crc >> 8) & 0xFF);

  return this->wrap_v5_request_(V5_FRAME_TYPE_INVERTER, V5_SENSOR_TYPE_MODBUS, modbus);
}

bool DeyeGen3Component::parse_response_(const std::vector<uint8_t> &frame, std::vector<uint8_t> &register_data) {
  std::vector<uint8_t> payload;
  if (!this->extract_v5_payload_(frame, payload))
    return false;

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

std::vector<uint8_t> DeyeGen3Component::build_write_request_(uint16_t reg, uint16_t value) {
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

bool DeyeGen3Component::parse_write_response_(const std::vector<uint8_t> &frame, uint16_t expected_reg,
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
// TCP transaction (blocking; runs on the background task, never on the main
// loop -- see run_task_()/loop() below)
// ---------------------------------------------------------------------------

bool DeyeGen3Component::connect_and_transact_(const std::vector<uint8_t> &request, std::vector<uint8_t> &response) {
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
        if (response.size() >= 3) {
          uint16_t payload_len = (uint16_t) response[1] | ((uint16_t) response[2] << 8);
          size_t expected_total = 11 + payload_len + 2;
          if (response.size() >= expected_total)
            break;
        }
      } else if (n == 0) {
        break;
      } else {
        break;
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

void DeyeGen3Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Deye GEN3 microinverter component...");
  if (this->logger_serial_ == 0) {
    ESP_LOGW(TAG,
             "logger_serial is 0 (default) -- on this author's TSUN GEN3 PLUS hardware "
             "this got NO response in client_mode, and the same is assumed (not yet "
             "confirmed) here. Set it to the inverter's real logger serial if polling fails.");
  }
  if (this->configured_model_ != DeyeGen3Model::MODEL_AUTO) {
    this->effective_model_ = this->configured_model_;
    this->model_resolved_ = true;
  }

  this->job_queue_ = xQueueCreate(4, sizeof(Job));
  this->result_queue_ = xQueueCreate(4, sizeof(JobResult *));
  if (this->job_queue_ == nullptr || this->result_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create job/result queues");
    this->mark_failed();
    return;
  }

  BaseType_t ok = xTaskCreate(&DeyeGen3Component::task_trampoline_, "deyegen3", 8192, this, 5, &this->task_handle_);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "Failed to create background task");
    this->mark_failed();
  }
}

void DeyeGen3Component::update() {
  Job job{JobType::POLL_READ};
  if (xQueueSend(this->job_queue_, &job, 0) != pdTRUE) {
    ESP_LOGW(TAG, "Background task busy, skipping this poll cycle");
  }
}

void DeyeGen3Component::loop() {
  JobResult *result = nullptr;
  while (xQueueReceive(this->result_queue_, &result, 0) == pdTRUE) {
    if (result != nullptr) {
      this->process_result_(result);
      delete result;
    }
  }
}

void DeyeGen3Component::task_trampoline_(void *param) {
  static_cast<DeyeGen3Component *>(param)->run_task_();
}

void DeyeGen3Component::run_task_() {
  Job job{};
  for (;;) {
    if (xQueueReceive(this->job_queue_, &job, portMAX_DELAY) != pdTRUE)
      continue;

    // Timing logged at DEBUG purely for diagnostics -- runs on the background
    // task, off the main thread, so it never shows up in the `debug`
    // component's loop_time sensor (confirmed with this exact pattern on
    // tsungen3's real hardware: 55-153 ms transactions, loop_time unaffected).
    uint32_t task_t0 = millis();

    auto *result = new JobResult();
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
        std::vector<uint8_t> request = this->build_write_request_(REG_ACTIVE_POWER_REGULATION, job.reg_value);
        std::vector<uint8_t> response;
        if (this->connect_and_transact_(request, response) &&
            this->parse_write_response_(response, REG_ACTIVE_POWER_REGULATION, job.reg_value)) {
          result->success = true;
        }
        break;
      }
    }

    ESP_LOGD(TAG, "Background transaction (job type %d) took %u ms", (int) job.type,
             (unsigned) (millis() - task_t0));

    if (xQueueSend(this->result_queue_, &result, 0) != pdTRUE) {
      delete result;
    }
  }
}

void DeyeGen3Component::process_result_(JobResult *result) {
  switch (result->type) {
    case JobType::POLL_READ: {
      if (!result->success) {
        ESP_LOGW(TAG, "Poll of %s:%u failed", this->host_.c_str(), this->port_);
        break;
      }
      if (!this->model_resolved_) {
        this->resolve_model_(result->register_data, REG_BLOCK_START);
      }
      this->handle_live_block_(result->register_data, REG_BLOCK_START, REG_BLOCK_COUNT);
      break;
    }
    case JobType::WRITE_POWER_PERCENT: {
      if (!result->success) {
        ESP_LOGW(TAG, "Failed to write power_percent (%u%%) to %s:%u", result->reg_value, this->host_.c_str(),
                 this->port_);
        break;
      }
      ESP_LOGI(TAG, "power_percent set to %u%% (register 0x%04X)", result->reg_value, REG_ACTIVE_POWER_REGULATION);
      break;
    }
  }
}

void DeyeGen3Component::resolve_model_(const std::vector<uint8_t> &regs, uint16_t start_reg) {
  // "Inverter ID" is 5 registers (10 bytes) of ASCII, 2 chars/register,
  // high byte first -- same byte order assumption as everything else here,
  // unconfirmed. Looks for a 4-MPPT model number substring; defaults to
  // (stays at) 2 MPPT if nothing matches, which is the conservative choice
  // (no PV3/PV4 readings rather than false/garbage ones).
  std::string id;
  for (uint8_t i = 0; i < REG_INVERTER_ID_COUNT; i++) {
    uint16_t reg = get_u16_(regs, REG_INVERTER_ID_START + i, start_reg);
    char hi = (char) ((reg >> 8) & 0xFF);
    char lo = (char) (reg & 0xFF);
    if (hi >= 0x20 && hi < 0x7F)
      id += hi;
    if (lo >= 0x20 && lo < 0x7F)
      id += lo;
  }

  bool is_4mppt = id.find("1300") != std::string::npos || id.find("1600") != std::string::npos ||
                  id.find("2000") != std::string::npos;
  this->effective_model_ = is_4mppt ? DeyeGen3Model::MODEL_4MPPT : DeyeGen3Model::MODEL_2MPPT;
  this->model_resolved_ = true;
  ESP_LOGI(TAG, "Inverter ID read as \"%s\" -- detected as %s (set `model:` explicitly if this is wrong)",
           id.c_str(), is_4mppt ? "4-MPPT" : "2-MPPT");
}

void DeyeGen3Component::handle_live_block_(const std::vector<uint8_t> &regs, uint16_t start_reg, uint16_t count) {
#ifdef USE_TEXT_SENSOR
  if (this->inverter_status_text_sensor_ != nullptr) {
    uint16_t status = get_u16_(regs, REG_RUNNING_STATUS, start_reg);
    const char *name = running_status_name_(status);
    if (name != nullptr) {
      this->inverter_status_text_sensor_->publish_state(name);
    } else {
      char hex[20];
      snprintf(hex, sizeof(hex), "Unknown (0x%04X)", status);
      this->inverter_status_text_sensor_->publish_state(hex);
    }
  }
#endif

#ifdef USE_SENSOR
  if (this->grid_voltage_sensor_ != nullptr)
    this->grid_voltage_sensor_->publish_state(get_u16_(regs, REG_AC_VOLTAGE, start_reg) * 0.1f);
  if (this->grid_current_sensor_ != nullptr)
    this->grid_current_sensor_->publish_state(get_s16_(regs, REG_GRID_CURRENT, start_reg) * 0.1f);
  if (this->grid_frequency_sensor_ != nullptr)
    this->grid_frequency_sensor_->publish_state(get_u16_(regs, REG_AC_FREQUENCY, start_reg) * 0.01f);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(((float) get_u16_(regs, REG_RADIATOR_TEMP, start_reg) - 1000.0f) *
                                              0.01f);
  if (this->rated_power_sensor_ != nullptr)
    this->rated_power_sensor_->publish_state(get_u16_(regs, REG_RATED_POWER, start_reg) * 0.1f);
  if (this->current_power_sensor_ != nullptr)
    this->current_power_sensor_->publish_state(
        get_u32_hi_first_(regs, REG_TOTAL_AC_POWER, REG_TOTAL_AC_POWER + 1, start_reg) * 0.1f);
  if (this->ac_energy_today_sensor_ != nullptr)
    this->ac_energy_today_sensor_->publish_state(get_u16_(regs, REG_DAILY_PRODUCTION, start_reg) * 0.1f);
  if (this->ac_energy_total_sensor_ != nullptr)
    this->ac_energy_total_sensor_->publish_state(
        get_u32_hi_first_(regs, REG_TOTAL_PRODUCTION, REG_TOTAL_PRODUCTION + 1, start_reg) * 0.1f);

  static const uint16_t PV_VOLTAGE_REG[4] = {REG_PV1_VOLTAGE, REG_PV2_VOLTAGE, REG_PV3_VOLTAGE, REG_PV4_VOLTAGE};
  static const uint16_t PV_CURRENT_REG[4] = {REG_PV1_CURRENT, REG_PV2_CURRENT, REG_PV3_CURRENT, REG_PV4_CURRENT};
  uint8_t pv_count = (this->effective_model_ == DeyeGen3Model::MODEL_4MPPT) ? 4 : 2;
  for (uint8_t i = 0; i < pv_count; i++) {
    float voltage = get_u16_(regs, PV_VOLTAGE_REG[i], start_reg) * 0.1f;
    float current = get_u16_(regs, PV_CURRENT_REG[i], start_reg) * 0.1f;
    if (this->pv_voltage_sensor_[i] != nullptr)
      this->pv_voltage_sensor_[i]->publish_state(voltage);
    if (this->pv_current_sensor_[i] != nullptr)
      this->pv_current_sensor_[i]->publish_state(current);
    // Computed, not read from a register -- no PV per-string power field
    // exists in the source register map for this protocol.
    if (this->pv_power_sensor_[i] != nullptr)
      this->pv_power_sensor_[i]->publish_state(voltage * current);
  }
#endif
}

void DeyeGen3Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Deye GEN3 microinverter:");
  ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Modbus address: %u", this->modbus_address_);
  ESP_LOGCONFIG(TAG, "  Logger serial: %u", (unsigned) this->logger_serial_);
  ESP_LOGCONFIG(TAG, "  Model: %s",
                this->configured_model_ == DeyeGen3Model::MODEL_AUTO
                    ? "auto (resolved at runtime from Inverter ID)"
                    : (this->configured_model_ == DeyeGen3Model::MODEL_4MPPT ? "deye_4mppt" : "deye_2mppt"));
}

// ---------------------------------------------------------------------------
// Control actions (number / output)
// ---------------------------------------------------------------------------

void DeyeGen3Component::set_power_percent(float percent) {
  if (percent < 0.0f)
    percent = 0.0f;
  if (percent > 100.0f)
    percent = 100.0f;

  // Active Power Regulations register is a direct 1%-per-unit value (not a
  // ratio like tsungen3's 100/1024 Output Coefficient) -- per the source
  // project's "scale: 1" for this field.
  uint16_t reg_value = (uint16_t) lroundf(percent);

  Job job{JobType::WRITE_POWER_PERCENT, reg_value};
  if (xQueueSend(this->job_queue_, &job, 0) != pdTRUE) {
    ESP_LOGW(TAG, "Background task busy, dropped power_percent write (%.1f%%)", percent);
  }
}

}  // namespace deyegen3
}  // namespace esphome
