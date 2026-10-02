#include "modbustcp.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::modbustcp {

static const char *const TAG = "modbustcp";

static inline uint16_t be16(const uint8_t *p) { return (uint16_t(p[0]) << 8) | uint16_t(p[1]); }

static std::string hex_string(const uint8_t *data, size_t len) {
  std::string out;
  out.reserve(len * 3);
  char buf[4];
  for (size_t i = 0; i < len; i++) {
    snprintf(buf, sizeof(buf), "%02X", data[i]);
    out += buf;
    if (i + 1 < len)
      out += ' ';
  }
  return out;
}

void ModbusTCP::setup() {
  this->client_ = new AsyncClient();

  // These callbacks run on the async_tcp task: keep them minimal (no calls into devices, no logging of frames).
  this->client_->onConnect(
      [](void *arg, AsyncClient *c) {
        auto *self = static_cast<ModbusTCP *>(arg);
        self->connected_ = true;
        self->reset_requested_ = true;
      },
      this);

  this->client_->onDisconnect(
      [](void *arg, AsyncClient *c) {
        auto *self = static_cast<ModbusTCP *>(arg);
        self->connected_ = false;
        self->reset_requested_ = true;
        self->last_attempt_ = 0;
      },
      this);

  this->client_->onError(
      [](void *arg, AsyncClient *c, int8_t err) {
        auto *self = static_cast<ModbusTCP *>(arg);
        self->connected_ = false;
        self->reset_requested_ = true;
      },
      this);

  this->client_->onData(
      [](void *arg, AsyncClient *c, void *data, size_t len) {
        auto *self = static_cast<ModbusTCP *>(arg);
        self->on_rx_(static_cast<const uint8_t *>(data), len);
      },
      this);
}

void ModbusTCP::set_host_and_reconnect(const std::string &host) {
  if (this->host_ == host && this->connected_) {
    return;  // same host and already connected: nothing to do
  }

  ESP_LOGI(TAG, "Changing host to %s...", host.c_str());
  this->host_ = host;

  if (this->client_ != nullptr) {
    if (this->client_->connected() || this->client_->connecting()) {
      this->client_->close(true);  // forced close of the active connection
    }
  }

  this->connected_ = false;
  this->reset_link_state_();
  this->last_attempt_ = 0;  // reconnect immediately
  this->connect();
}

void ModbusTCP::connect() {
  if (this->client_ != nullptr && !this->client_->connecting() && !this->client_->connected()) {
    if (!this->client_->connect(this->host_.c_str(), this->port_)) {
      ESP_LOGW(TAG, "Connection to %s:%u failed, will retry...", this->host_.c_str(), this->port_);
    }
  }
}

void ModbusTCP::reset_link_state_() {
  // Forget the request in flight and any partial frame: the reply (if any) belonged to the old connection.
  this->awaiting_response_ = false;
  this->waiting_for_response = 0;
  this->pending_rx_.clear();
  LockGuard lock(this->rx_mutex_);
  this->rx_buffer_.clear();
}

void ModbusTCP::on_rx_(const uint8_t *data, size_t len) {
  LockGuard lock(this->rx_mutex_);
  if (this->rx_buffer_.size() + len > MAX_RX_BUFFER) {
    // Never happens with a well behaved server (one reply per request); resynchronise from scratch.
    this->rx_buffer_.clear();
    this->reset_requested_ = true;
    return;
  }
  this->rx_buffer_.insert(this->rx_buffer_.end(), data, data + len);
}

void ModbusTCP::process_rx_() {
  if (this->reset_requested_.exchange(false)) {
    ESP_LOGD(TAG, "Link state changed (connected=%s)", this->connected_ ? "yes" : "no");
    this->reset_link_state_();
  }

  {
    LockGuard lock(this->rx_mutex_);
    if (!this->rx_buffer_.empty()) {
      this->pending_rx_.insert(this->pending_rx_.end(), this->rx_buffer_.begin(), this->rx_buffer_.end());
      this->rx_buffer_.clear();
    }
  }

  while (this->pending_rx_.size() >= MBAP_HEADER_SIZE) {
    const uint8_t *p = this->pending_rx_.data();
    const uint16_t protocol = be16(p + 2);
    const uint16_t length = be16(p + 4);  // unit id + PDU
    if (protocol != 0 || length < 2 || length > MAX_ADU_SIZE - 6) {
      ESP_LOGW(TAG, "Invalid MBAP header (protocol=%u length=%u), dropping the connection to resynchronise", protocol,
               length);
      this->pending_rx_.clear();
      if (this->client_ != nullptr)
        this->client_->close(true);
      return;
    }
    const size_t total = 6 + length;
    if (this->pending_rx_.size() < total)
      break;  // wait for the rest of the frame
    this->handle_frame_(p, total);
    this->pending_rx_.erase(this->pending_rx_.begin(), this->pending_rx_.begin() + total);
  }

  if (this->pending_rx_.size() > MAX_RX_BUFFER) {
    this->pending_rx_.clear();
  }
}

void ModbusTCP::handle_frame_(const uint8_t *frame, size_t len) {
  const uint16_t tid = be16(frame);
  const uint8_t unit = frame[6];
  const uint8_t function = frame[7];

  ESP_LOGD(TAG, "<<< %s", hex_string(frame, len).c_str());

  if (!this->awaiting_response_ || tid != this->pending_tid_) {
    ESP_LOGD(TAG, "Ignoring reply with transaction id %u (in flight: %s %u)", tid,
             this->awaiting_response_ ? "id" : "none", this->awaiting_response_ ? this->pending_tid_ : 0);
    return;
  }
  if ((function & 0x7F) != this->pending_function_) {
    ESP_LOGW(TAG, "Reply function 0x%02X does not match the request (0x%02X), ignoring", function,
             this->pending_function_);
    return;
  }

  // Extract the payload handed to the devices
  const uint8_t *data = nullptr;
  size_t data_len = 0;
  const bool is_error = (function & 0x80) != 0;
  if (is_error) {
    if (len < 9) {
      ESP_LOGW(TAG, "Truncated exception reply");
      return;
    }
  } else if (function >= 0x01 && function <= 0x04) {
    // read: [byte count][data...]
    const size_t byte_count = frame[8];
    if (len < 9 || 9 + byte_count > len) {
      ESP_LOGW(TAG, "Truncated read reply (byte count %u, frame %u bytes)", (unsigned) byte_count, (unsigned) len);
      return;
    }
    data = frame + 9;
    data_len = byte_count;
  } else if (function == 0x05 || function == 0x06 || function == 0x0F || function == 0x10) {
    // write: echo of the address and value / quantity
    if (len < 12) {
      ESP_LOGW(TAG, "Truncated write reply");
      return;
    }
    data = frame + 8;
    data_len = 4;
  } else {
    data = frame + 8;
    data_len = len - 8;
  }

  // The reply closes the transaction: the next request can go out immediately.
  const uint8_t request_unit = this->pending_unit_;
  this->awaiting_response_ = false;
  this->waiting_for_response = 0;

  if (is_error) {
    static const char *const names[] = {"?",
                                        "ILLEGAL FUNCTION",
                                        "ILLEGAL DATA ADDRESS",
                                        "ILLEGAL DATA VALUE",
                                        "SERVER DEVICE FAILURE",
                                        "ACKNOWLEDGE",
                                        "SERVER DEVICE BUSY"};
    const uint8_t code = frame[8];
    ESP_LOGE(TAG, "Exception 0x%02X %s (unit %u, function 0x%02X)", code, code <= 6 ? names[code] : "?", unit,
             function & 0x7F);
    for (auto *device : this->devices_) {
      if (device->address_ == request_unit)
        device->on_modbus_error(function & 0x7F, code);
    }
    return;
  }

  const std::vector<uint8_t> payload(data, data + data_len);
  for (auto *device : this->devices_) {
    if (device->address_ == request_unit)
      device->on_modbus_data(payload);
  }
}

void ModbusTCP::on_shutdown() {
  if (this->client_ != nullptr) {
    this->client_->close();
  }
}

void ModbusTCP::loop() {
  this->process_rx_();

  const uint32_t now = millis();

  if (!this->connected_) {
    if (now - this->last_attempt_ > 5000) {
      this->last_attempt_ = now;
      ESP_LOGW(TAG, "Reconnecting to %s:%u...", this->host_.c_str(), this->port_);
      this->connect();
    }
  }

  if (this->awaiting_response_ && now - this->last_send_ > this->send_wait_time_) {
    ESP_LOGD(TAG, "No reply to transaction %u within %u ms", this->pending_tid_, this->send_wait_time_);
    this->awaiting_response_ = false;
    this->waiting_for_response = 0;
  }
}

void ModbusTCP::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Modbus_TCP:\n"
                "  Client: %s\n"
                "  Port: %u\n"
                "  Send Wait Time: %u ms",
                this->host_.c_str(), this->port_, this->send_wait_time_);
}

float ModbusTCP::get_setup_priority() const { return setup_priority::AFTER_WIFI - 1.0f; }

bool ModbusTCP::write_frame_(const uint8_t *frame, size_t len) {
  if (!this->connected_ || this->client_ == nullptr || !this->client_->canSend()) {
    ESP_LOGW(TAG, "Cannot send, not connected");
    return false;
  }
  ESP_LOGD(TAG, ">>> %s", hex_string(frame, len).c_str());
  return this->client_->write(reinterpret_cast<const char *>(frame), len) == len;
}

void ModbusTCP::send_pdu_(uint8_t unit, const uint8_t *pdu, size_t pdu_len) {
  if (pdu_len == 0 || MBAP_HEADER_SIZE + pdu_len > MAX_ADU_SIZE) {
    ESP_LOGE(TAG, "Invalid PDU size %u", (unsigned) pdu_len);
    return;
  }

  uint8_t frame[MAX_ADU_SIZE];
  const uint16_t tid = this->transaction_id_++;
  frame[0] = tid >> 8;
  frame[1] = tid & 0xFF;
  frame[2] = 0x00;
  frame[3] = 0x00;
  const uint16_t length = static_cast<uint16_t>(1 + pdu_len);  // unit id + PDU
  frame[4] = length >> 8;
  frame[5] = length & 0xFF;
  frame[6] = unit;
  memcpy(frame + 7, pdu, pdu_len);

  // Even when the frame cannot be written (link down) the request is marked as in flight, so the device sees the
  // usual timeout and counts the attempt (retries / offline detection) instead of spinning.
  this->write_frame_(frame, MBAP_HEADER_SIZE + pdu_len);

  this->pending_tid_ = tid;
  this->pending_unit_ = unit;
  this->pending_function_ = pdu[0] & 0x7F;
  this->awaiting_response_ = true;
  this->waiting_for_response = unit != 0 ? unit : 0xFF;
  this->last_send_ = millis();
}

void ModbusTCP::send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
                     uint8_t payload_len, const uint8_t *payload) {
  static const size_t MAX_VALUES = 128;
  if (number_of_entities > MAX_VALUES && function_code <= 0x10) {
    ESP_LOGE(TAG, "send too many values %u max=%u", number_of_entities, (unsigned) MAX_VALUES);
    return;
  }

  uint8_t pdu[MAX_ADU_SIZE - MBAP_HEADER_SIZE];
  size_t pos = 0;
  pdu[pos++] = function_code;
  pdu[pos++] = start_address >> 8;
  pdu[pos++] = start_address & 0xFF;

  if (function_code == 0x05 || function_code == 0x06) {
    // single write: the value (2 bytes) replaces the quantity field
    if (payload == nullptr || payload_len < 2) {
      ESP_LOGE(TAG, "Function 0x%02X needs a 2 byte value", function_code);
      return;
    }
    pdu[pos++] = payload[0];
    pdu[pos++] = payload[1];
  } else {
    pdu[pos++] = number_of_entities >> 8;
    pdu[pos++] = number_of_entities & 0xFF;
    if (function_code == 0x0F || function_code == 0x10) {
      if (payload == nullptr || payload_len == 0) {
        ESP_LOGE(TAG, "Function 0x%02X needs a payload", function_code);
        return;
      }
      if (pos + 1 + payload_len > sizeof(pdu)) {
        ESP_LOGE(TAG, "Payload too large to send: %u bytes", payload_len);
        return;
      }
      pdu[pos++] = payload_len;  // byte count
      memcpy(pdu + pos, payload, payload_len);
      pos += payload_len;
    }
  }

  this->send_pdu_(address, pdu, pos);
}

void ModbusTCP::send_raw(const std::vector<uint8_t> &payload) {
  if (payload.size() < 2) {
    return;  // needs at least a unit id and a function code
  }
  this->send_pdu_(payload[0], payload.data() + 1, payload.size() - 1);
}

}  // namespace esphome::modbustcp
