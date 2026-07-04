#include "ryuw122.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ryuw122 {

static const char *const TAG = "ryuw122";

// ----------------------------------------------------------------------
// Setup / configuration state machine
// ----------------------------------------------------------------------
// All AT+ setters are pushed one at a time at boot, waiting for +OK before
// moving on to the next parameter. This avoids overrunning the module's
// UART RX buffer, which was a recurring failure mode on the RYLR998
// component when several commands were fired back to back.

void RYUW122Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RYUW122...");
  this->rx_buffer_.reserve(MAX_RX_BUFFER);
  this->advance_setup_();
}

void RYUW122Component::advance_setup_() {
  char buf[96];

  switch (this->setup_state_) {
    case SetupState::BEGIN:
      this->setup_state_ = SetupState::SET_MODE;
      this->enqueue_command_("AT");
      break;
    case SetupState::SET_MODE:
      snprintf(buf, sizeof(buf), "AT+MODE=%d", static_cast<int>(this->mode_));
      this->enqueue_command_(buf);
      this->setup_state_ = SetupState::SET_NETWORKID;
      break;
    case SetupState::SET_NETWORKID:
      snprintf(buf, sizeof(buf), "AT+NETWORKID=%s", this->network_id_.c_str());
      this->enqueue_command_(buf);
      this->setup_state_ = SetupState::SET_ADDRESS;
      break;
    case SetupState::SET_ADDRESS:
      snprintf(buf, sizeof(buf), "AT+ADDRESS=%s", this->address_.c_str());
      this->enqueue_command_(buf);
      this->setup_state_ = SetupState::SET_CPIN;
      break;
    case SetupState::SET_CPIN:
      if (this->has_password_) {
        snprintf(buf, sizeof(buf), "AT+CPIN=%s", this->password_.c_str());
        this->enqueue_command_(buf);
      }
      this->setup_state_ = SetupState::SET_CHANNEL;
      if (!this->has_password_) {
        this->advance_setup_();
        return;
      }
      break;
    case SetupState::SET_CHANNEL:
      snprintf(buf, sizeof(buf), "AT+CHANNEL=%d", this->channel_);
      this->enqueue_command_(buf);
      this->setup_state_ = SetupState::SET_BANDWIDTH;
      break;
    case SetupState::SET_BANDWIDTH:
      snprintf(buf, sizeof(buf), "AT+BANDWIDTH=%d", this->bandwidth_);
      this->enqueue_command_(buf);
      this->setup_state_ = SetupState::SET_CRFOP;
      break;
    case SetupState::SET_CRFOP:
      snprintf(buf, sizeof(buf), "AT+CRFOP=%d", this->tx_power_);
      this->enqueue_command_(buf);
      this->setup_state_ = SetupState::SET_CAL;
      break;
    case SetupState::SET_CAL:
      snprintf(buf, sizeof(buf), "AT+CAL=%d", this->calibration_);
      this->enqueue_command_(buf);
      this->setup_state_ = SetupState::SET_RSSI;
      break;
    case SetupState::SET_RSSI:
      snprintf(buf, sizeof(buf), "AT+RSSI=%d", this->rssi_enable_ ? 1 : 0);
      this->enqueue_command_(buf);
      this->setup_state_ = SetupState::SET_TAGD;
      break;
    case SetupState::SET_TAGD:
      // Only meaningful in TAG mode, but harmless to set regardless.
      snprintf(buf, sizeof(buf), "AT+TAGD=%u,%u", (unsigned) this->tag_rf_enable_ms_,
               (unsigned) this->tag_rf_disable_ms_);
      this->enqueue_command_(buf);
      this->setup_state_ = SetupState::DONE;
      break;
    case SetupState::DONE:
      ESP_LOGCONFIG(TAG, "RYUW122 configuration complete");
      break;
  }
}

// ----------------------------------------------------------------------
// Command queue / rate limiting
// ----------------------------------------------------------------------

void RYUW122Component::enqueue_command_(const std::string &cmd) {
  this->command_queue_.push_back({cmd});
}

void RYUW122Component::send_next_command_() {
  if (this->waiting_for_response_)
    return;
  if (this->command_queue_.empty())
    return;

  uint32_t now = millis();
  if (now - this->last_command_sent_ms_ < MIN_COMMAND_INTERVAL_MS)
    return;

  const RYUW122PendingCommand &pending = this->command_queue_.front();
  ESP_LOGV(TAG, "TX: %s", pending.command.c_str());
  this->write_str(pending.command.c_str());
  this->write_str("\r\n");
  this->waiting_for_response_ = true;
  this->last_command_sent_ms_ = now;
}

// ----------------------------------------------------------------------
// Main loop: non-blocking line-based UART parsing
// ----------------------------------------------------------------------

void RYUW122Component::loop() {
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);

    if (c == '\n') {
      // Trim trailing \r if present
      if (!this->rx_buffer_.empty() && this->rx_buffer_.back() == '\r')
        this->rx_buffer_.pop_back();
      if (!this->rx_buffer_.empty()) {
        this->process_line_(this->rx_buffer_);
      }
      this->rx_buffer_.clear();
    } else if (c == '\r') {
      // ignore, handled above via \n
    } else {
      if (this->rx_buffer_.size() < MAX_RX_BUFFER) {
        this->rx_buffer_.push_back(static_cast<char>(c));
      } else {
        // Guard against a runaway buffer (e.g. noise on the line);
        // drop and resync on the next newline.
        ESP_LOGW(TAG, "RX buffer overflow, discarding line");
        this->rx_buffer_.clear();
      }
    }
  }

  // Command response timeout: don't get stuck forever if the module
  // never replies (e.g. wrong baud rate, module asleep).
  if (this->waiting_for_response_ && (millis() - this->last_command_sent_ms_ > this->command_timeout_ms_)) {
    ESP_LOGW(TAG, "Timeout waiting for response to: %s",
             this->command_queue_.empty() ? "?" : this->command_queue_.front().command.c_str());
    this->waiting_for_response_ = false;
    if (!this->command_queue_.empty())
      this->command_queue_.pop_front();
    // Setup commands still need to progress even after a timeout, otherwise
    // a single dropped reply would stall configuration forever.
    if (this->setup_state_ != SetupState::DONE)
      this->advance_setup_();
  }

  this->send_next_command_();
}

// ----------------------------------------------------------------------
// Line dispatch
// ----------------------------------------------------------------------

void RYUW122Component::process_line_(const std::string &line) {
  ESP_LOGV(TAG, "RX: %s", line.c_str());

  if (line == "+OK") {
    this->handle_ok_();
  } else if (line.rfind("+ERR=", 0) == 0) {
    this->handle_err_(line);
  } else if (line == "+RESET") {
    ESP_LOGI(TAG, "Module is resetting");
  } else if (line == "+READY") {
    ESP_LOGI(TAG, "Module ready");
  } else if (line.rfind("+ANCHOR_RCV=", 0) == 0) {
    this->handle_anchor_rcv_(line);
  } else if (line.rfind("+TAG_RCV=", 0) == 0) {
    this->handle_tag_rcv_(line);
  } else if (line.rfind("+VER=", 0) == 0) {
    ESP_LOGI(TAG, "Firmware version: %s", line.c_str() + 5);
  } else if (line.rfind("+FACTORY", 0) == 0) {
    ESP_LOGI(TAG, "Factory reset acknowledged");
  } else if (line.rfind("+MODE=", 0) == 0 || line.rfind("+NETWORKID=", 0) == 0 ||
             line.rfind("+ADDRESS=", 0) == 0 || line.rfind("+CPIN=", 0) == 0 ||
             line.rfind("+CHANNEL=", 0) == 0 || line.rfind("+BANDWIDTH=", 0) == 0 ||
             line.rfind("+CRFOP=", 0) == 0 || line.rfind("+CAL=", 0) == 0 ||
             line.rfind("+RSSI=", 0) == 0 || line.rfind("+TAGD=", 0) == 0 ||
             line.rfind("+UID=", 0) == 0 || line.rfind("+IPR=", 0) == 0) {
    // Query (AT+xxx?) echo, informational only.
    ESP_LOGD(TAG, "Setting readback: %s", line.c_str());
  } else {
    ESP_LOGD(TAG, "Unhandled line: %s", line.c_str());
  }
}

void RYUW122Component::handle_ok_() {
  if (!this->command_queue_.empty())
    this->command_queue_.pop_front();
  this->waiting_for_response_ = false;

  if (this->setup_state_ != SetupState::DONE) {
    this->advance_setup_();
  }
}

void RYUW122Component::handle_err_(const std::string &line) {
  ESP_LOGW(TAG, "Module reported error: %s (last command: %s)", line.c_str(),
           this->command_queue_.empty() ? "?" : this->command_queue_.front().command.c_str());
  if (!this->command_queue_.empty())
    this->command_queue_.pop_front();
  this->waiting_for_response_ = false;

  if (this->setup_state_ != SetupState::DONE) {
    this->advance_setup_();
  }
}

// ----------------------------------------------------------------------
// +ANCHOR_RCV=<TAG Address>,<PAYLOAD LENGTH>,<TAG DATA>,<DISTANCE>[,<RSSI>]
//   example: +ANCHOR_RCV=DAVID123,5,HELLO,40 cm
// The DATA field is ASCII and could in theory contain a comma, so we use
// the announced PAYLOAD LENGTH to slice it out precisely instead of doing
// a naive split on every comma.
// ----------------------------------------------------------------------

void RYUW122Component::handle_anchor_rcv_(const std::string &line) {
  // Strip the "+ANCHOR_RCV=" prefix
  std::string rest = line.substr(strlen("+ANCHOR_RCV="));

  size_t comma1 = rest.find(',');
  if (comma1 == std::string::npos) {
    ESP_LOGW(TAG, "Malformed +ANCHOR_RCV: %s", line.c_str());
    return;
  }
  std::string tag_address = rest.substr(0, comma1);

  size_t comma2 = rest.find(',', comma1 + 1);
  if (comma2 == std::string::npos) {
    ESP_LOGW(TAG, "Malformed +ANCHOR_RCV: %s", line.c_str());
    return;
  }
  std::string len_str = rest.substr(comma1 + 1, comma2 - comma1 - 1);
  int payload_len = atoi(len_str.c_str());
  if (payload_len < 0 || payload_len > 12) {
    ESP_LOGW(TAG, "Malformed +ANCHOR_RCV payload length: %s", line.c_str());
    return;
  }

  size_t data_start = comma2 + 1;
  if (data_start + static_cast<size_t>(payload_len) > rest.size()) {
    ESP_LOGW(TAG, "Malformed +ANCHOR_RCV, truncated data: %s", line.c_str());
    return;
  }
  std::string data = rest.substr(data_start, payload_len);

  size_t after_data = data_start + payload_len;
  if (after_data >= rest.size() || rest[after_data] != ',') {
    ESP_LOGW(TAG, "Malformed +ANCHOR_RCV, missing distance field: %s", line.c_str());
    return;
  }
  std::string remainder = rest.substr(after_data + 1);

  // remainder is "<DISTANCE>[,<RSSI>]" — distance may look like "40 cm" or "40"
  float distance = 0.0f;
  int rssi = 0;
  size_t comma3 = remainder.find(',');
  std::string distance_str = comma3 == std::string::npos ? remainder : remainder.substr(0, comma3);
  distance = strtof(distance_str.c_str(), nullptr);
  if (comma3 != std::string::npos) {
    std::string rssi_str = remainder.substr(comma3 + 1);
    rssi = atoi(rssi_str.c_str());
  }

  ESP_LOGD(TAG, "ANCHOR_RCV from %s: '%s' distance=%.1fcm rssi=%d", tag_address.c_str(), data.c_str(),
           distance, rssi);

#ifdef USE_SENSOR
  if (this->distance_sensor_ != nullptr)
    this->distance_sensor_->publish_state(distance);
  if (this->rssi_sensor_ != nullptr && this->rssi_enable_)
    this->rssi_sensor_->publish_state(rssi);
#endif
#ifdef USE_TEXT_SENSOR
  if (this->last_data_text_sensor_ != nullptr)
    this->last_data_text_sensor_->publish_state(data);
#endif

  this->anchor_rcv_callback_.call(tag_address, data, distance, rssi);
}

// ----------------------------------------------------------------------
// +TAG_RCV=<PAYLOAD LENGTH>,<DATA>[,<RSSI>]
//   example: +TAG_RCV=4,TEST
// ----------------------------------------------------------------------

void RYUW122Component::handle_tag_rcv_(const std::string &line) {
  std::string rest = line.substr(strlen("+TAG_RCV="));

  size_t comma1 = rest.find(',');
  if (comma1 == std::string::npos) {
    ESP_LOGW(TAG, "Malformed +TAG_RCV: %s", line.c_str());
    return;
  }
  std::string len_str = rest.substr(0, comma1);
  int payload_len = atoi(len_str.c_str());
  if (payload_len < 0 || payload_len > 12) {
    ESP_LOGW(TAG, "Malformed +TAG_RCV payload length: %s", line.c_str());
    return;
  }

  size_t data_start = comma1 + 1;
  if (data_start + static_cast<size_t>(payload_len) > rest.size()) {
    ESP_LOGW(TAG, "Malformed +TAG_RCV, truncated data: %s", line.c_str());
    return;
  }
  std::string data = rest.substr(data_start, payload_len);

  int rssi = 0;
  size_t after_data = data_start + payload_len;
  if (after_data < rest.size() && rest[after_data] == ',') {
    std::string rssi_str = rest.substr(after_data + 1);
    rssi = atoi(rssi_str.c_str());
  }

  ESP_LOGD(TAG, "TAG_RCV: '%s' rssi=%d", data.c_str(), rssi);

#ifdef USE_SENSOR
  if (this->rssi_sensor_ != nullptr && this->rssi_enable_)
    this->rssi_sensor_->publish_state(rssi);
#endif
#ifdef USE_TEXT_SENSOR
  if (this->last_data_text_sensor_ != nullptr)
    this->last_data_text_sensor_->publish_state(data);
#endif

  this->tag_rcv_callback_.call(data, rssi);
}

// ----------------------------------------------------------------------
// Public send API
// ----------------------------------------------------------------------

void RYUW122Component::tag_send(const std::string &payload) {
  if (payload.size() > 12) {
    ESP_LOGE(TAG, "tag_send: payload too long (%zu bytes, max 12)", payload.size());
    return;
  }
  char buf[48];
  snprintf(buf, sizeof(buf), "AT+TAG_SEND=%zu,%s", payload.size(), payload.c_str());
  this->enqueue_command_(buf);
}

void RYUW122Component::anchor_send(const std::string &tag_address, const std::string &payload) {
  if (tag_address.empty() || tag_address.size() > 8) {
    ESP_LOGE(TAG, "anchor_send: tag_address must be 1-8 ASCII characters");
    return;
  }
  if (payload.size() > 12) {
    ESP_LOGE(TAG, "anchor_send: payload too long (%zu bytes, max 12)", payload.size());
    return;
  }
  char buf[64];
  snprintf(buf, sizeof(buf), "AT+ANCHOR_SEND=%s,%zu,%s", tag_address.c_str(), payload.size(),
           payload.c_str());
  this->enqueue_command_(buf);
}

// ----------------------------------------------------------------------

void RYUW122Component::dump_config() {
  ESP_LOGCONFIG(TAG, "RYUW122:");
  ESP_LOGCONFIG(TAG, "  Mode: %s",
                this->mode_ == RYUW122_MODE_ANCHOR ? "ANCHOR"
                : this->mode_ == RYUW122_MODE_TAG  ? "TAG"
                                                    : "SLEEP");
  ESP_LOGCONFIG(TAG, "  Network ID: %s", this->network_id_.c_str());
  ESP_LOGCONFIG(TAG, "  Address: %s", this->address_.c_str());
  ESP_LOGCONFIG(TAG, "  Password set: %s", YESNO(this->has_password_));
  ESP_LOGCONFIG(TAG, "  Channel: %d", this->channel_);
  ESP_LOGCONFIG(TAG, "  Bandwidth: %s", this->bandwidth_ == 1 ? "6.8 Mbps" : "850 Kbps");
  ESP_LOGCONFIG(TAG, "  TX Power index: %d", this->tx_power_);
  ESP_LOGCONFIG(TAG, "  Calibration: %d cm", this->calibration_);
  ESP_LOGCONFIG(TAG, "  RSSI reporting: %s", YESNO(this->rssi_enable_));
  if (this->mode_ == RYUW122_MODE_TAG) {
    ESP_LOGCONFIG(TAG, "  TAG duty cycle: enable=%ums disable=%ums", (unsigned) this->tag_rf_enable_ms_,
                  (unsigned) this->tag_rf_disable_ms_);
  }
  this->check_uart_settings(115200);
}

}  // namespace ryuw122
}  // namespace esphome
