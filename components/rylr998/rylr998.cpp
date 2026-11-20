#include "rylr998.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

#include <cmath>
#include <cstdio>
#include <cstring>

namespace esphome::rylr998 {

static const char *const TAG = "rylr998";

// Boot / command timing
static constexpr uint32_t BOOT_WAIT_MS = 500;        // let the module boot after power-up
static constexpr uint32_t CMD_TIMEOUT_MS = 1000;     // answer timeout of an AT command
static constexpr uint32_t CONFIG_GAP_MS = 50;        // pause between configuration commands
static constexpr uint32_t TX_GUARD_MARGIN_MS = 30;   // UART transfer + module processing
static constexpr uint32_t LEGACY_GUARD_MS = 600;     // fixed guard when air_time: false
static constexpr uint32_t HEALTH_INTERVAL_MS = 60000;  // idle time before an "AT" health check
static constexpr uint8_t CONFIG_STEPS = 6;           // AT, ADDRESS, BAND, PARAMETER, NETWORKID, CRFOP
static constexpr uint8_t MAX_FAILURES = 3;           // consecutive timeouts before re-initialising
static constexpr uint8_t MAX_TX_RETRIES = 3;         // retries of a packet refused with +ERR=17
static constexpr uint8_t MAX_POWER_RETRIES = 3;      // retries of AT+CRFOP

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

// Wrap-safe "has `target` been reached" for timestamps closer than ~24 days.
static inline bool reached_(uint32_t now, uint32_t target) { return static_cast<int32_t>(now - target) >= 0; }

// Parses a decimal integer in [b, e). Surrounding blanks are ignored. Never throws, never allocates.
static bool parse_int_(const char *b, const char *e, int &out) {
  while (b < e && (*b == ' ' || *b == '\t')) b++;
  while (e > b && (e[-1] == ' ' || e[-1] == '\t')) e--;
  if (b == e) return false;
  bool neg = false;
  if (*b == '-' || *b == '+') {
    neg = (*b == '-');
    b++;
  }
  if (b == e || (e - b) > 9) return false;
  int v = 0;
  for (; b < e; b++) {
    if (*b < '0' || *b > '9') return false;
    v = v * 10 + (*b - '0');
  }
  out = neg ? -v : v;
  return true;
}

static inline int hex_val_(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static const char *err_text_(int code) {
  switch (code) {
    case 1: return "missing CR/LF";
    case 2: return "command does not start with AT";
    case 4: return "unknown command";
    case 5: return "data length mismatch";
    case 10: return "TX timeout";
    case 12: return "CRC error (RX)";
    case 13: return "TX data exceeds 240 bytes";
    case 14: return "flash write failed";
    case 15: return "unknown failure";
    case 17: return "last TX not completed";
    case 18: return "preamble value not allowed";
    case 19: return "RX header error";
    case 20: return "smart receiving mode time not allowed";
    default: return "unknown error code";
  }
}

// ---------------------------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------------------------

void RYLR998Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RYLR998...");
  this->rx_data_.reserve(MAX_PAYLOAD_BYTES);

  const uint8_t max_power = this->max_tx_power_();
  if (this->tx_power_ > max_power) {
    ESP_LOGW(TAG, "tx_power (%u dBm) exceeds the %u dBm maximum of %s, clamped", (unsigned) this->tx_power_,
             (unsigned) max_power, this->variant_name_());
    this->tx_power_ = max_power;
  }

  // Nothing blocks here: the module is configured asynchronously from loop().
  const uint32_t now = millis();
  this->state_ = State::BOOT_WAIT;
  this->boot_deadline_ = now + BOOT_WAIT_MS;
  this->note_activity_(now);
}

void RYLR998Component::loop() {
  this->read_uart_(millis());
  this->service_(millis());
}

void RYLR998Component::dump_config() {
  ESP_LOGCONFIG(TAG, "RYLR998:");
  ESP_LOGCONFIG(TAG, "  Variant: %s", this->variant_name_());
  ESP_LOGCONFIG(TAG, "  Address: %u", (unsigned) this->address_);
  ESP_LOGCONFIG(TAG, "  Frequency: %lu Hz", (unsigned long) this->frequency_);
  ESP_LOGCONFIG(TAG, "  Spreading Factor: %u", (unsigned) this->spreading_factor_);
  ESP_LOGCONFIG(TAG, "  Bandwidth: %s", this->bandwidth_to_string_(this->bandwidth_));
  ESP_LOGCONFIG(TAG, "  Coding Rate: 4/%u", (unsigned) (this->coding_rate_ + 4));
  ESP_LOGCONFIG(TAG, "  Preamble Length: %u", (unsigned) this->preamble_length_);
  ESP_LOGCONFIG(TAG, "  Network ID: %u", (unsigned) this->network_id_);
  ESP_LOGCONFIG(TAG, "  TX Power: %u dBm", (unsigned) this->tx_power_);
  if (this->compute_air_time_) {
    ESP_LOGCONFIG(TAG, "  TX guard: computed per packet (e.g. %lu ms for 16 bytes, %lu ms for %u bytes)",
                  (unsigned long) this->compute_air_time_ms(16 * 2), (unsigned long) this->compute_air_time_ms(MAX_PAYLOAD_CHARS),
                  (unsigned) MAX_PAYLOAD_BYTES);
  } else {
    ESP_LOGCONFIG(TAG, "  TX guard: fixed %lu ms", (unsigned long) LEGACY_GUARD_MS);
  }
  ESP_LOGCONFIG(TAG, "  TX queue: %u packets of up to %u bytes", (unsigned) TX_QUEUE_SIZE, (unsigned) MAX_PAYLOAD_BYTES);
  if (!this->initialized_) {
    ESP_LOGW(TAG, "  Module not ready (state %u)", (unsigned) this->state_);
  }
}

// ---------------------------------------------------------------------------------------------
// Air time (Semtech SX126x formula, explicit header, CRC on)
// ---------------------------------------------------------------------------------------------

uint32_t RYLR998Component::compute_air_time_ms(size_t chars_sent) const {
  if (!this->compute_air_time_) return LEGACY_GUARD_MS;

  const float sf = static_cast<float>(this->spreading_factor_);
  const float tsym = powf(2.0f, sf) / static_cast<float>(this->bandwidth_);  // seconds
  const float de = tsym > 0.016f ? 1.0f : 0.0f;                              // low data rate optimisation
  const float payload = static_cast<float>(chars_sent + MODULE_OVERHEAD_BYTES);

  float n = ceilf((8.0f * payload - 4.0f * sf + 28.0f + 16.0f) / (4.0f * (sf - 2.0f * de)));
  if (n < 0.0f) n = 0.0f;
  const float payload_symbols = 8.0f + n * static_cast<float>(this->coding_rate_ + 4);
  const float preamble_symbols = static_cast<float>(this->preamble_length_) + (sf >= 7.0f ? 4.25f : 6.25f);

  return static_cast<uint32_t>(ceilf((preamble_symbols + payload_symbols) * tsym * 1000.0f));
}

// ---------------------------------------------------------------------------------------------
// RX
// ---------------------------------------------------------------------------------------------

void RYLR998Component::read_uart_(uint32_t now) {
  (void) now;
  uint8_t c;
  while (this->available() > 0 && this->read_byte(&c)) {
    if (c == '\n') {
      if (this->rx_overflow_) {
        ESP_LOGW(TAG, "Oversized line discarded (> %u bytes)", (unsigned) RX_LINE_MAX);
      } else if (this->rx_len_ > 0) {
        this->rx_line_[this->rx_len_] = '\0';
        this->process_line_(millis());
      }
      this->rx_len_ = 0;
      this->rx_overflow_ = false;
    } else if (c == '\r' || this->rx_overflow_) {
      continue;  // ignore CR, and everything up to the next LF once a line overflowed
    } else if (this->rx_len_ < RX_LINE_MAX) {
      this->rx_line_[this->rx_len_++] = static_cast<char>(c);
    } else {
      this->rx_overflow_ = true;  // drop the whole line instead of parsing a truncated one
    }
  }
}

void RYLR998Component::process_line_(uint32_t now) {
  const char *line = this->rx_line_;
  const size_t n = this->rx_len_;
  ESP_LOGV(TAG, "RX: %s", line);
  this->note_activity_(now);

  // +RCV is tested first: its payload must never be mistaken for a command answer.
  if (strncmp(line, "+RCV=", 5) == 0) {
    this->handle_rcv_(line + 5, n - 5);
    return;
  }
  if (strncmp(line, "+OK", 3) == 0) {
    this->on_ok_(now);
    return;
  }
  if (strncmp(line, "+ERR=", 5) == 0) {
    int code = -1;
    const char *p = line + 5;
    if (*p >= '0' && *p <= '9') {
      code = 0;
      for (int i = 0; i < 4 && *p >= '0' && *p <= '9'; i++, p++) code = code * 10 + (*p - '0');
    }
    this->on_error_(code, now);
    return;
  }
  if (strncmp(line, "+READY", 6) == 0 || strncmp(line, "+RESET", 6) == 0) {
    this->on_module_ready_(now);
    return;
  }
  ESP_LOGV(TAG, "Ignored line: %s", line);
}

void RYLR998Component::handle_rcv_(const char *p, size_t n) {
  // +RCV=<address>,<length>,<data>,<rssi>,<snr>   (the datasheet shows blanks after the commas)
  const char *end = p + n;
  const char *c1 = static_cast<const char *>(memchr(p, ',', n));
  const char *c2 = c1 ? static_cast<const char *>(memchr(c1 + 1, ',', end - (c1 + 1))) : nullptr;
  if (c2 == nullptr) {
    ESP_LOGW(TAG, "Malformed +RCV (missing fields)");
    return;
  }
  // RSSI and SNR are the last two fields: search from the end because ASCII data may contain commas.
  const char *c_snr = nullptr;
  const char *c_rssi = nullptr;
  for (const char *q = end - 1; q > c2; q--) {
    if (*q == ',') {
      if (c_snr == nullptr) {
        c_snr = q;
      } else {
        c_rssi = q;
        break;
      }
    }
  }
  if (c_snr == nullptr || c_rssi == nullptr) {
    ESP_LOGW(TAG, "Malformed +RCV (missing RSSI/SNR)");
    return;
  }

  int address = 0, declared = 0, rssi = 0, snr = 0;
  if (!parse_int_(p, c1, address) || address < 0 || address > 65535 || !parse_int_(c1 + 1, c2, declared) ||
      !parse_int_(c_rssi + 1, c_snr, rssi) || !parse_int_(c_snr + 1, end, snr)) {
    ESP_LOGW(TAG, "Malformed +RCV (invalid number)");
    return;
  }

  const char *db = c2 + 1;
  const char *de = c_rssi;
  if (static_cast<int>(de - db) != declared) {
    // Tolerate blanks around the data field, as printed in the datasheet example.
    while (db < de && *db == ' ') db++;
    while (de > db && de[-1] == ' ') de--;
    if (static_cast<int>(de - db) != declared) {
      ESP_LOGW(TAG, "Malformed +RCV (declared length %d, got %d)", declared, static_cast<int>(de - db));
      return;
    }
  }

  // Data sent by this component is hex-encoded. Anything else (raw ASCII from another firmware) is
  // delivered as-is.
  const size_t dl = static_cast<size_t>(de - db);
  bool all_hex = dl > 0 && (dl % 2 == 0);
  for (size_t i = 0; all_hex && i < dl; i++) {
    if (hex_val_(db[i]) < 0) all_hex = false;
  }
  this->rx_data_.clear();
  if (all_hex) {
    for (size_t i = 0; i < dl; i += 2) {
      this->rx_data_.push_back(static_cast<uint8_t>((hex_val_(db[i]) << 4) | hex_val_(db[i + 1])));
    }
  } else {
    this->rx_data_.assign(reinterpret_cast<const uint8_t *>(db), reinterpret_cast<const uint8_t *>(de));
  }

  this->publish_packet_(static_cast<uint16_t>(address), rssi, snr);
}

void RYLR998Component::publish_packet_(uint16_t address, int rssi, int snr) {
  ESP_LOGD(TAG, "RCV from %u: %u bytes (RSSI %d, SNR %d)", (unsigned) address, (unsigned) this->rx_data_.size(), rssi,
           snr);
  const float rssi_f = static_cast<float>(rssi);
  const float snr_f = static_cast<float>(snr);

  if (this->raw_cb_ != nullptr) this->raw_cb_(this->rx_data_, rssi_f, snr_f);
  for (auto *trigger : this->packet_triggers_) trigger->trigger(this->rx_data_, rssi_f, snr_f);
  this->packet_callback_.call(address, this->rx_data_, rssi, snr);

#ifdef USE_SENSOR
  if (this->rssi_sensor_ != nullptr) this->rssi_sensor_->publish_state(rssi_f);
  if (this->snr_sensor_ != nullptr) this->snr_sensor_->publish_state(snr_f);
#endif
}

// ---------------------------------------------------------------------------------------------
// Command engine
//
// One AT command is in flight at a time (the module requires the +OK of a command before the next
// one). Answers are matched in loop(); nothing here ever waits, so other components keep running
// and incoming +RCV frames are never discarded while a command is pending.
// ---------------------------------------------------------------------------------------------

void RYLR998Component::note_activity_(uint32_t now) { this->next_ping_at_ = now + HEALTH_INTERVAL_MS; }

void RYLR998Component::service_(uint32_t now) {
  if (this->tx_busy_ && reached_(now, this->tx_busy_until_)) this->tx_busy_ = false;
  if (this->gap_active_ && reached_(now, this->next_cmd_not_before_)) this->gap_active_ = false;

  if (this->cmd_kind_ != CmdKind::NONE) {
    if (reached_(now, this->cmd_deadline_)) this->on_timeout_(now);
    return;
  }
  if (this->gap_active_) return;
  this->dispatch_next_(now);
}

void RYLR998Component::dispatch_next_(uint32_t now) {
  if (this->state_ == State::BOOT_WAIT) {
    if (reached_(now, this->boot_deadline_)) {
      this->state_ = State::CONFIG;
      this->cfg_step_ = 0;
    }
    return;
  }
  if (this->state_ == State::CONFIG) {
    if (this->reset_requested_) {
      this->send_reset_(now);
    } else {
      this->send_config_step_(now);
    }
    return;
  }
  // State::READY
  if (this->reset_requested_) {
    this->send_reset_(now);
  } else if (this->tx_power_dirty_) {
    this->send_tx_power_(now);
  } else if (this->tx_count_ > 0 && !this->tx_busy_) {
    this->send_next_packet_(now);
  } else if (!this->tx_busy_ && reached_(now, this->next_ping_at_)) {
    this->send_ping_(now);
  }
}

void RYLR998Component::start_command_(CmdKind kind, size_t len, uint32_t now, uint32_t timeout_ms) {
  (void) now;
  this->cmd_buf_[len] = '\0';
  ESP_LOGV(TAG, "TX: %s", this->cmd_buf_);
  this->cmd_buf_[len] = '\r';
  this->cmd_buf_[len + 1] = '\n';
  this->cmd_buf_[len + 2] = '\0';
  this->write_str(this->cmd_buf_);
  this->cmd_kind_ = kind;
  this->cmd_deadline_ = millis() + timeout_ms;  // counted from the end of the UART transfer
}

void RYLR998Component::send_config_step_(uint32_t now) {
  const size_t max_len = CMD_BUF_SIZE - 3;
  int n = 0;
  switch (this->cfg_step_) {
    case 0:
      n = snprintf(this->cmd_buf_, max_len, "AT");
      break;
    case 1:
      n = snprintf(this->cmd_buf_, max_len, "AT+ADDRESS=%u", (unsigned) this->address_);
      break;
    case 2:
      n = snprintf(this->cmd_buf_, max_len, "AT+BAND=%lu", (unsigned long) this->frequency_);
      break;
    case 3:
      n = snprintf(this->cmd_buf_, max_len, "AT+PARAMETER=%u,%u,%u,%u", (unsigned) this->spreading_factor_,
                   (unsigned) this->bandwidth_to_code_(this->bandwidth_), (unsigned) this->coding_rate_,
                   (unsigned) this->preamble_length_);
      break;
    case 4:
      n = snprintf(this->cmd_buf_, max_len, "AT+NETWORKID=%u", (unsigned) this->network_id_);
      break;
    default:
      this->tx_power_dirty_ = false;  // the value sent here is the current one
      n = snprintf(this->cmd_buf_, max_len, "AT+CRFOP=%u", (unsigned) this->tx_power_);
      break;
  }
  this->start_command_(CmdKind::CONFIG, static_cast<size_t>(n), now, CMD_TIMEOUT_MS);
}

void RYLR998Component::send_tx_power_(uint32_t now) {
  this->tx_power_dirty_ = false;
  const int n = snprintf(this->cmd_buf_, CMD_BUF_SIZE - 3, "AT+CRFOP=%u", (unsigned) this->tx_power_);
  ESP_LOGD(TAG, "Setting TX power to %u dBm", (unsigned) this->tx_power_);
  this->start_command_(CmdKind::TXPOWER, static_cast<size_t>(n), now, CMD_TIMEOUT_MS);
}

void RYLR998Component::send_reset_(uint32_t now) {
  this->reset_requested_ = false;
  this->reset_expected_ = true;
  const int n = snprintf(this->cmd_buf_, CMD_BUF_SIZE - 3, "AT+RESET");
  ESP_LOGI(TAG, "Resetting the module");
  // The module answers "+RESET" then "+READY" (no +OK); see on_module_ready_().
  this->start_command_(CmdKind::RESET, static_cast<size_t>(n), now, CMD_TIMEOUT_MS);
}

void RYLR998Component::send_ping_(uint32_t now) {
  const int n = snprintf(this->cmd_buf_, CMD_BUF_SIZE - 3, "AT");
  ESP_LOGV(TAG, "Health check");
  this->start_command_(CmdKind::PING, static_cast<size_t>(n), now, CMD_TIMEOUT_MS);
}

void RYLR998Component::send_next_packet_(uint32_t now) {
  static const char HEX_DIGITS[] = "0123456789ABCDEF";
  const TxPacket &pk = this->tx_queue_[this->tx_head_];

  // Hex-encode straight into the command buffer: no intermediate buffer, no heap.
  int n = snprintf(this->cmd_buf_, CMD_BUF_SIZE - 3, "AT+SEND=%u,%u,", (unsigned) pk.destination,
                   (unsigned) (pk.length * 2));
  size_t len = static_cast<size_t>(n);
  for (uint8_t i = 0; i < pk.length; i++) {
    this->cmd_buf_[len++] = HEX_DIGITS[pk.data[i] >> 4];
    this->cmd_buf_[len++] = HEX_DIGITS[pk.data[i] & 0x0F];
  }

  const uint32_t air_ms = this->compute_air_time_ms(static_cast<size_t>(pk.length) * 2);
  ESP_LOGD(TAG, "TX to %u: %u bytes, about %lu ms on air", (unsigned) pk.destination, (unsigned) pk.length,
           (unsigned long) air_ms);
  // The answer may come only once the frame has been sent, hence the air time in the timeout.
  this->start_command_(CmdKind::SEND, len, now, air_ms + CMD_TIMEOUT_MS);

  // The radio is busy from now on, whether or not the module answers quickly.
  this->tx_busy_ = true;
  this->tx_busy_until_ = millis() + air_ms + TX_GUARD_MARGIN_MS;
}

void RYLR998Component::finish_command_(uint32_t now, uint32_t gap_ms) {
  this->cmd_kind_ = CmdKind::NONE;
  if (gap_ms > 0) {
    this->gap_active_ = true;
    this->next_cmd_not_before_ = now + gap_ms;
  }
}

void RYLR998Component::pop_packet_() {
  if (this->tx_count_ > 0) {
    this->tx_head_ = (this->tx_head_ + 1) % TX_QUEUE_SIZE;
    this->tx_count_--;
  }
}

void RYLR998Component::advance_config_(uint32_t now) {
  this->cfg_attempts_ = 0;
  this->cfg_step_++;
  if (this->cfg_step_ < CONFIG_STEPS) return;

  this->state_ = State::READY;
  this->initialized_ = true;
  this->consecutive_failures_ = 0;
  this->status_clear_warning();
  this->note_activity_(now);
  ESP_LOGI(TAG, "RYLR998 ready (%s, %lu Hz, SF%u, %u dBm)", this->variant_name_(), (unsigned long) this->frequency_,
           (unsigned) this->spreading_factor_, (unsigned) this->tx_power_);
#ifdef USE_NUMBER
  if (this->tx_power_number_ != nullptr) this->tx_power_number_->publish_state(static_cast<float>(this->tx_power_));
#endif
#ifdef USE_SENSOR
  if (this->first_ready_ && this->last_error_sensor_ != nullptr) this->last_error_sensor_->publish_state(0.0f);
#endif
  this->first_ready_ = false;
}

void RYLR998Component::restart_config_(uint32_t now, uint32_t delay_ms) {
  this->state_ = State::CONFIG;
  this->cfg_step_ = 0;
  this->cfg_attempts_ = 0;
  this->initialized_ = false;
  this->cmd_kind_ = CmdKind::NONE;
  this->consecutive_failures_ = 0;
  this->gap_active_ = true;
  this->next_cmd_not_before_ = now + delay_ms;
  this->status_set_warning("RYLR998 not ready");
}

void RYLR998Component::on_ok_(uint32_t now) {
  this->consecutive_failures_ = 0;
  switch (this->cmd_kind_) {
    case CmdKind::NONE:
      ESP_LOGV(TAG, "Unsolicited +OK");
      return;
    case CmdKind::PING:
    case CmdKind::RESET:  // some firmwares answer +OK before +RESET
      this->finish_command_(now, 0);
      return;
    case CmdKind::SEND:
      this->pop_packet_();
      this->finish_command_(now, 0);
      return;
    case CmdKind::TXPOWER:
      this->tx_power_retries_ = 0;
      ESP_LOGI(TAG, "TX power set to %u dBm", (unsigned) this->tx_power_);
#ifdef USE_NUMBER
      if (this->tx_power_number_ != nullptr) this->tx_power_number_->publish_state(static_cast<float>(this->tx_power_));
#endif
      this->finish_command_(now, CONFIG_GAP_MS);
      return;
    case CmdKind::CONFIG:
      this->finish_command_(now, CONFIG_GAP_MS);
      this->advance_config_(now);
      return;
  }
}

void RYLR998Component::on_error_(int code, uint32_t now) {
#ifdef USE_SENSOR
  if (code >= 0 && this->last_error_sensor_ != nullptr) this->last_error_sensor_->publish_state(static_cast<float>(code));
#endif
  // +ERR=12 (CRC) and +ERR=19 (header) report a failed *reception*: they are not the answer of the
  // command in flight.
  if (code == 12 || code == 19) {
    ESP_LOGD(TAG, "Reception error +ERR=%d: %s", code, err_text_(code));
    return;
  }
  this->consecutive_failures_ = 0;  // the module answered, so it is alive

  switch (this->cmd_kind_) {
    case CmdKind::NONE:
      ESP_LOGW(TAG, "Unsolicited +ERR=%d: %s", code, err_text_(code));
      return;
    case CmdKind::PING:
      this->finish_command_(now, 0);
      return;
    case CmdKind::RESET:
      ESP_LOGW(TAG, "AT+RESET refused, +ERR=%d: %s", code, err_text_(code));
      this->reset_expected_ = false;
      this->finish_command_(now, CONFIG_GAP_MS);
      return;
    case CmdKind::SEND: {
      TxPacket &pk = this->tx_queue_[this->tx_head_];
      if (code == 17 && pk.retries < MAX_TX_RETRIES) {
        pk.retries++;
        ESP_LOGD(TAG, "Module still transmitting (+ERR=17), retry %u/%u", (unsigned) pk.retries,
                 (unsigned) MAX_TX_RETRIES);
        this->tx_busy_ = true;
        // Wait for the transmission in progress to end: half an air time plus a growing back-off.
        this->tx_busy_until_ = now + this->compute_air_time_ms(static_cast<size_t>(pk.length) * 2) / 2 + 100u * pk.retries;
      } else {
        ESP_LOGW(TAG, "Packet dropped, +ERR=%d: %s", code, err_text_(code));
        this->pop_packet_();
      }
      this->finish_command_(now, 0);
      return;
    }
    case CmdKind::TXPOWER:
      ESP_LOGW(TAG, "Setting TX power failed, +ERR=%d: %s", code, err_text_(code));
      if (++this->tx_power_retries_ < MAX_POWER_RETRIES) this->tx_power_dirty_ = true;
      this->finish_command_(now, CONFIG_GAP_MS);
      return;
    case CmdKind::CONFIG:
      ESP_LOGW(TAG, "Configuration step %u rejected, +ERR=%d: %s", (unsigned) this->cfg_step_, code, err_text_(code));
      this->finish_command_(now, CONFIG_GAP_MS);
      this->advance_config_(now);  // keep going, like a failed setting always did
      return;
  }
}

void RYLR998Component::on_timeout_(uint32_t now) {
  const CmdKind kind = this->cmd_kind_;
  this->consecutive_failures_++;

  switch (kind) {
    case CmdKind::SEND:
      ESP_LOGW(TAG, "No answer to AT+SEND, packet dropped");
      this->pop_packet_();
      this->finish_command_(now, CONFIG_GAP_MS);
      break;
    case CmdKind::RESET:
      ESP_LOGW(TAG, "No answer to AT+RESET");
      this->reset_expected_ = false;
      this->finish_command_(now, CONFIG_GAP_MS);
      break;
    case CmdKind::PING:
      ESP_LOGW(TAG, "No answer to health check (%u/%u)", (unsigned) this->consecutive_failures_,
               (unsigned) MAX_FAILURES);
      this->finish_command_(now, 500);
      break;
    case CmdKind::TXPOWER:
      ESP_LOGW(TAG, "No answer to AT+CRFOP");
      if (++this->tx_power_retries_ < MAX_POWER_RETRIES) this->tx_power_dirty_ = true;
      this->finish_command_(now, CONFIG_GAP_MS);
      break;
    case CmdKind::CONFIG:
      if (this->cfg_step_ == 0) {
        // The module does not answer at all: retry every second, then every 10 s.
        this->cfg_attempts_++;
        ESP_LOGW(TAG, "RYLR998 does not answer (attempt %u), check wiring, power and baud rate",
                 (unsigned) this->cfg_attempts_);
        if (this->cfg_attempts_ >= 3) this->status_set_warning("RYLR998 not responding");
        if (this->cfg_attempts_ >= 250) this->cfg_attempts_ = 5;
        this->consecutive_failures_ = 0;
        this->finish_command_(now, this->cfg_attempts_ < 5 ? 1000 : 10000);
        return;
      }
      ESP_LOGW(TAG, "No answer to configuration step %u, continuing", (unsigned) this->cfg_step_);
      this->finish_command_(now, CONFIG_GAP_MS);
      this->advance_config_(now);
      break;
    case CmdKind::NONE:
      break;
  }

  if (this->consecutive_failures_ >= MAX_FAILURES) {
    ESP_LOGE(TAG, "Module stopped answering, re-initialising it");
    this->restart_config_(now, 1000);
  }
}

void RYLR998Component::on_module_ready_(uint32_t now) {
  if (this->state_ == State::BOOT_WAIT) {
    ESP_LOGD(TAG, "Module booted (+READY)");
    this->boot_deadline_ = now + 100;
    return;
  }
  if (this->reset_expected_) {
    // Answer to our AT+RESET: "+RESET" arrives first (state READY/CONFIG), then "+READY" once the
    // module has booted. Settings not stored in its flash are lost, so the configuration is re-sent.
    if (this->state_ == State::READY) {
      ESP_LOGI(TAG, "Module reset (+RESET), re-applying the configuration");
    } else {
      ESP_LOGD(TAG, "Module booted after reset (+READY)");
      this->reset_expected_ = false;
    }
  } else if (this->state_ == State::READY) {
    // The module restarted on its own (supply dip, brown-out...) and lost every setting that is
    // not stored in its flash (frequency, RF parameters, power).
    ESP_LOGW(TAG, "Module restarted unexpectedly (+READY), re-applying the configuration");
  } else {
    ESP_LOGD(TAG, "+READY during configuration, restarting it");
  }
  // After our own AT+RESET the module is still booting when "+RESET" arrives: wait for "+READY"
  // (which shortens this delay) instead of sending commands it would ignore.
  this->restart_config_(now, this->reset_expected_ ? 3000 : 100);
}

// ---------------------------------------------------------------------------------------------
// TX API
// ---------------------------------------------------------------------------------------------

bool RYLR998Component::transmit_packet(const std::vector<uint8_t> &data) { return this->transmit_packet(0, data); }

bool RYLR998Component::transmit_packet(uint16_t destination, const std::vector<uint8_t> &data) {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "Module not ready, packet refused");
    return false;
  }
  if (data.empty()) {
    ESP_LOGW(TAG, "Empty packet refused");
    return false;
  }
  if (data.size() > MAX_PAYLOAD_BYTES) {
    ESP_LOGE(TAG, "Payload too large: %u bytes (maximum %u: the module accepts 240 hex characters)",
             (unsigned) data.size(), (unsigned) MAX_PAYLOAD_BYTES);
    return false;
  }
  if (this->tx_count_ >= TX_QUEUE_SIZE) {
    ESP_LOGW(TAG, "TX queue full, packet refused (sending faster than the radio can transmit)");
    return false;
  }

  TxPacket &pk = this->tx_queue_[(this->tx_head_ + this->tx_count_) % TX_QUEUE_SIZE];
  pk.destination = destination;
  pk.length = static_cast<uint8_t>(data.size());
  pk.retries = 0;
  memcpy(pk.data, data.data(), data.size());
  this->tx_count_++;
  return true;
}

bool RYLR998Component::send_data(uint16_t destination, const std::vector<uint8_t> &data) {
  return this->transmit_packet(destination, data);
}

bool RYLR998Component::send_data(uint16_t destination, const std::string &data) {
  return this->transmit_packet(destination, std::vector<uint8_t>(data.begin(), data.end()));
}

void RYLR998Component::apply_tx_power(uint8_t power) {
  const uint8_t max_power = this->max_tx_power_();
  if (power > max_power) {
    ESP_LOGW(TAG, "Requested TX power %u dBm exceeds the %s maximum (%u dBm), clamping", (unsigned) power,
             this->variant_name_(), (unsigned) max_power);
    power = max_power;
  }
  this->tx_power_ = power;
  this->tx_power_retries_ = 0;
  this->tx_power_dirty_ = true;  // sent by loop() when the module is idle; never blocks the caller
}

// ---------------------------------------------------------------------------------------------
// Bandwidth helpers
// ---------------------------------------------------------------------------------------------

uint8_t RYLR998Component::bandwidth_to_code_(uint32_t bandwidth) {
  switch (bandwidth) {
    case 125000: return 7;
    case 250000: return 8;
    case 500000: return 9;
    default:
      ESP_LOGW(TAG, "Unknown bandwidth %lu Hz, defaulting to 125 kHz", (unsigned long) bandwidth);
      return 7;
  }
}

const char *RYLR998Component::bandwidth_to_string_(uint32_t bandwidth) {
  switch (bandwidth) {
    case 125000: return "125 kHz";
    case 250000: return "250 kHz";
    case 500000: return "500 kHz";
    default: return "Unknown";
  }
}

}  // namespace esphome::rylr998
