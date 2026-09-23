#include "hmsw.h"
#include "esphome/core/log.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include <cstring>
#include <cstdio>
#include <cerrno>

namespace esphome {
namespace hmsw {

static const char *const TAG = "hmsw";

// ---------------------------------------------------------------------------
// CRC-16/MODBUS -- poly 0x8005 (reflected 0xA001), init 0xFFFF, no xorout.
// Matches protcol.md (henkwiedig/Hoymiles-DTU-Proto) and the
// crcmod.mkCrcFun(0x18005, rev=True, initCrc=0xFFFF, xorOut=0x0000) call in
// suaveolent/hoymiles-wifi's DTU.generate_message()/parse_response().
// ---------------------------------------------------------------------------
uint16_t HMSWComponent::crc16_modbus_(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void HMSWComponent::set_persistent_power_limit_percent(float percent) {
  if (percent < 0.0f) percent = 0.0f;
  if (percent > 100.0f) percent = 100.0f;
  this->power_limit_value_ = percent;
  this->power_limit_pending_ = true;
}

void HMSWComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HMSW (host=%s:%u)...", this->host_.c_str(), this->port_);
  this->last_poll_ = millis() - this->poll_interval_ms_;  // poll soon after boot
}

// ---------------------------------------------------------------------------
// Frame construction -- header (10 bytes) + protobuf payload. Only the
// unencrypted, non-"extended" variant is implemented (see hmsw.h and
// README.md). `out` must be at least payload_len + FRAME_HEADER_SIZE bytes.
// ---------------------------------------------------------------------------
size_t HMSWComponent::build_frame_(const uint8_t *cmd, const uint8_t *payload, size_t payload_len, uint8_t *out) {
  this->sequence_++;
  uint16_t crc = crc16_modbus_(payload, payload_len);
  uint16_t total_len = static_cast<uint16_t>(payload_len + FRAME_HEADER_SIZE);

  out[0] = 'H';
  out[1] = 'M';
  out[2] = cmd[0];
  out[3] = cmd[1];
  out[4] = static_cast<uint8_t>(this->sequence_ >> 8);
  out[5] = static_cast<uint8_t>(this->sequence_ & 0xFF);
  out[6] = static_cast<uint8_t>(crc >> 8);
  out[7] = static_cast<uint8_t>(crc & 0xFF);
  out[8] = static_cast<uint8_t>(total_len >> 8);
  out[9] = static_cast<uint8_t>(total_len & 0xFF);
  if (payload_len > 0) memcpy(out + FRAME_HEADER_SIZE, payload, payload_len);
  return total_len;
}

void HMSWComponent::start_request_(RequestKind kind) {
  if (this->state_ != ConnState::IDLE) {
    ESP_LOGV(TAG, "start_request_: a request is already in flight, ignoring");
    return;
  }

  uint8_t payload[128];
  pb_ostream_t stream = pb_ostream_from_buffer(payload, sizeof(payload));
  bool ok = false;
  const uint8_t *cmd = nullptr;

  if (kind == RequestKind::HEARTBEAT) {
    HBResDTO msg = HBResDTO_init_zero;
    msg.offset = 0;
    msg.time = static_cast<int32_t>(time(nullptr));
    ok = pb_encode(&stream, HBResDTO_fields, &msg);
    cmd = CMD_HB;
  } else if (kind == RequestKind::REAL_DATA) {
    RealDataResDTO msg = RealDataResDTO_init_zero;
    msg.package_now = 0;
    msg.error_code = 0;
    msg.offset = 0;
    msg.time = static_cast<int32_t>(time(nullptr));
    ok = pb_encode(&stream, RealDataResDTO_fields, &msg);
    cmd = CMD_REAL_DATA;
  } else if (kind == RequestKind::REAL_DATA_NEW) {
    // Ported from suaveolent/hoymiles-wifi's async_get_real_data_new():
    // `offset` is a fixed 28800 (8h in seconds -- the same constant the
    // Python client uses for every OFFSET-bearing request, not something
    // derived from our actual timezone), `cp` is the requested page
    // (0 on the first request of a poll cycle, see loop()/handle_real_data_new_()).
    static const int32_t REAL_DATA_OFFSET = 28800;
    RealDataNewResDTO msg = RealDataNewResDTO_init_zero;
    msg.offset = REAL_DATA_OFFSET;
    msg.time = static_cast<int32_t>(time(nullptr));
    msg.cp = this->real_data_new_cp_;
    ok = pb_encode(&stream, RealDataNewResDTO_fields, &msg);
    cmd = CMD_REAL_DATA_NEW;
  } else if (kind == RequestKind::POWER_LIMIT) {
    CommandResDTO msg = CommandResDTO_init_zero;
    msg.time = static_cast<int32_t>(time(nullptr));
    msg.action = CMD_ACTION_LIMIT_POWER;
    msg.package_nub = 1;
    msg.tid = static_cast<int64_t>(time(nullptr));
    // "A:<permille>,B:0,C:0\r" -- ported verbatim from
    // suaveolent/hoymiles-wifi's async_set_power_limit() (ChannelA is the
    // only one used for a single-MPPT-group relative limit; B/C are for
    // multi-string models this component doesn't target yet).
    int permille = static_cast<int>(this->power_limit_value_ * 10.0f + 0.5f);
    snprintf(msg.data, sizeof(msg.data), "A:%d,B:0,C:0\r", permille);
    ok = pb_encode(&stream, CommandResDTO_fields, &msg);
    cmd = CMD_COMMAND;
  } else {
    return;
  }

  if (!ok) {
    ESP_LOGW(TAG, "Failed to encode request payload (%s)", PB_GET_ERROR(&stream));
    return;
  }

  this->tx_len_ = this->build_frame_(cmd, payload, stream.bytes_written, this->tx_buf_);
  this->tx_sent_ = 0;
  this->rx_len_ = 0;
  this->rx_expected_len_ = 0;
  this->pending_kind_ = kind;

  this->socket_ = socket::socket_ip(SOCK_STREAM, IPPROTO_TCP);
  if (!this->socket_) {
    ESP_LOGW(TAG, "Failed to allocate socket");
    return;
  }
  this->socket_->setblocking(false);

  struct sockaddr_storage addr;
  socklen_t addrlen = socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr),
                                            this->host_.c_str(), this->port_);
  if (addrlen == 0) {
    ESP_LOGW(TAG, "Could not resolve/parse host '%s'", this->host_.c_str());
    this->socket_ = nullptr;
    return;
  }

  int err = this->socket_->connect(reinterpret_cast<struct sockaddr *>(&addr), addrlen);
  if (err != 0 && errno != EINPROGRESS) {
    ESP_LOGW(TAG, "connect() failed immediately (errno=%d)", errno);
    this->socket_ = nullptr;
    return;
  }

  this->state_ = ConnState::CONNECTING;
  this->state_deadline_ = millis() + this->request_timeout_ms_;
  const char *kind_name = "realtime-data";
  if (kind == RequestKind::HEARTBEAT) kind_name = "heartbeat";
  else if (kind == RequestKind::POWER_LIMIT) kind_name = "power-limit command";
  else if (kind == RequestKind::REAL_DATA_NEW) kind_name = "realtime-data-new";
  ESP_LOGV(TAG, "Connecting to %s:%u for %s request", this->host_.c_str(), this->port_, kind_name);
}

void HMSWComponent::abort_request_(const char *reason) {
  if (reason != nullptr) {
    ESP_LOGD(TAG, "Request aborted: %s", reason);
  }
  if (this->socket_) {
    this->socket_->close();
    this->socket_ = nullptr;
  }
  this->state_ = ConnState::IDLE;
  this->pending_kind_ = RequestKind::NONE;
}

void HMSWComponent::handle_connecting_() {
  // Non-blocking connect() completion: writability indicates the connect
  // attempt has resolved (success or failure) -- same convention as a
  // classic BSD-socket select()-on-writable check.
  if (!this->socket_->ready()) {
    if (millis() > this->state_deadline_) this->abort_request_("connect timeout");
    return;
  }
  this->state_ = ConnState::SENDING;
  this->state_deadline_ = millis() + this->request_timeout_ms_;
}

void HMSWComponent::handle_sending_() {
  while (this->tx_sent_ < this->tx_len_) {
    ssize_t n = this->socket_->write(this->tx_buf_ + this->tx_sent_, this->tx_len_ - this->tx_sent_);
    if (n > 0) {
      this->tx_sent_ += static_cast<size_t>(n);
      continue;
    }
    if (n == 0 || (n < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))) {
      if (millis() > this->state_deadline_) this->abort_request_("send timeout");
      return;  // try again next tick
    }
    this->abort_request_("send() error");
    return;
  }
  this->state_ = ConnState::RECEIVING;
  this->state_deadline_ = millis() + this->request_timeout_ms_;
}

void HMSWComponent::handle_receiving_() {
  while (true) {
    if (this->rx_len_ >= MAX_FRAME_SIZE) {
      this->abort_request_("response too large");
      return;
    }
    ssize_t n = this->socket_->read(this->rx_buf_ + this->rx_len_, MAX_FRAME_SIZE - this->rx_len_);
    if (n > 0) {
      this->rx_len_ += static_cast<size_t>(n);
    } else if (n == 0) {
      // Peer closed the connection -- normal for this protocol (one
      // connection per request/response, like the Python reference client).
      break;
    } else {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        if (millis() > this->state_deadline_) {
          this->abort_request_("receive timeout");
        }
        return;  // try again next tick
      }
      this->abort_request_("recv() error");
      return;
    }
  }

  // Connection closed -- parse whatever we accumulated.
  if (this->rx_len_ < FRAME_HEADER_SIZE) {
    ESP_LOGD(TAG, "Response too short (%u bytes) -- discarded", (unsigned) this->rx_len_);
    this->consecutive_failures_++;
    this->publish_reachable_(this->consecutive_failures_ < REACHABLE_FAILURE_THRESHOLD);
    this->abort_request_(nullptr);
    return;
  }

  if (this->rx_buf_[0] != 'H' || this->rx_buf_[1] != 'M') {
    ESP_LOGD(TAG, "Bad magic in response header -- discarded");
    this->consecutive_failures_++;
    this->publish_reachable_(this->consecutive_failures_ < REACHABLE_FAILURE_THRESHOLD);
    this->abort_request_(nullptr);
    return;
  }

  const uint8_t cmd[2] = {this->rx_buf_[2], this->rx_buf_[3]};
  uint16_t crc_target = (static_cast<uint16_t>(this->rx_buf_[6]) << 8) | this->rx_buf_[7];
  uint16_t total_len = (static_cast<uint16_t>(this->rx_buf_[8]) << 8) | this->rx_buf_[9];

  if (total_len > this->rx_len_) {
    ESP_LOGD(TAG, "Truncated response (declared %u bytes, got %u) -- discarded", total_len,
             (unsigned) this->rx_len_);
    this->consecutive_failures_++;
    this->publish_reachable_(this->consecutive_failures_ < REACHABLE_FAILURE_THRESHOLD);
    this->abort_request_(nullptr);
    return;
  }

  const uint8_t *payload = this->rx_buf_ + FRAME_HEADER_SIZE;
  size_t payload_len = total_len - FRAME_HEADER_SIZE;
  uint16_t crc_actual = crc16_modbus_(payload, payload_len);
  if (crc_actual != crc_target) {
    ESP_LOGW(TAG, "CRC16 mismatch (got 0x%04X, expected 0x%04X) -- discarded", crc_actual, crc_target);
    this->consecutive_failures_++;
    this->publish_reachable_(this->consecutive_failures_ < REACHABLE_FAILURE_THRESHOLD);
    this->abort_request_(nullptr);
    return;
  }

  this->on_frame_received_(cmd, payload, payload_len);
  this->abort_request_(nullptr);
}

void HMSWComponent::on_frame_received_(const uint8_t *cmd, const uint8_t *payload, size_t len) {
  this->consecutive_failures_ = 0;
  this->publish_reachable_(true);

  if (cmd[0] == CMD_REAL_DATA[0] && cmd[1] == CMD_REAL_DATA[1]) {
    RealDataReqDTO data = RealDataReqDTO_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);
    if (!pb_decode(&stream, RealDataReqDTO_fields, &data)) {
      ESP_LOGW(TAG, "Failed to decode RealDataReqDTO (%s)", PB_GET_ERROR(&stream));
      return;
    }
    this->handle_real_data_(data);
  } else if (cmd[0] == CMD_REAL_DATA_NEW[0] && cmd[1] == CMD_REAL_DATA_NEW[1]) {
    RealDataNewReqDTO data = RealDataNewReqDTO_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);
    if (!pb_decode(&stream, RealDataNewReqDTO_fields, &data)) {
      ESP_LOGW(TAG, "Failed to decode RealDataNewReqDTO (%s)", PB_GET_ERROR(&stream));
      return;
    }
    this->handle_real_data_new_(data);
  } else if (cmd[0] == CMD_HB[0] && cmd[1] == CMD_HB[1]) {
    ESP_LOGV(TAG, "Heartbeat acknowledged");
  } else if (cmd[0] == CMD_COMMAND[0] && cmd[1] == CMD_COMMAND[1]) {
    CommandReqDTO data = CommandReqDTO_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);
    if (!pb_decode(&stream, CommandReqDTO_fields, &data)) {
      ESP_LOGW(TAG, "Failed to decode CommandReqDTO (%s)", PB_GET_ERROR(&stream));
      return;
    }
    this->handle_command_response_(data);
  } else {
    ESP_LOGV(TAG, "Unhandled response command 0x%02X 0x%02X (%u bytes payload)", cmd[0], cmd[1], (unsigned) len);
  }
}

void HMSWComponent::handle_real_data_(const RealDataReqDTO &data) {
  ESP_LOGD(TAG, "Realtime data: %u PV channel(s), csq=%d", (unsigned) data.pv_data_count, (int) data.csq);

#ifdef USE_SENSOR
  for (pb_size_t i = 0; i < data.pv_data_count && i < 4; i++) {
    const PvDataMO &pv = data.pv_data[i];
    // Ported field scaling from suaveolent/hoymiles-wifi's higher-level
    // helpers: voltage/current/power/temperature are reported x10 (one
    // implied decimal digit), same convention as the HM/HMS RF protocol.
    if (this->dc_voltage_[i]) this->dc_voltage_[i]->publish_state(pv.pv_vol / 10.0f);
    if (this->dc_current_[i]) this->dc_current_[i]->publish_state(pv.pv_cur / 100.0f);
    if (this->dc_power_[i]) this->dc_power_[i]->publish_state(pv.pv_power / 10.0f);
    if (this->dc_energy_total_[i]) this->dc_energy_total_[i]->publish_state(static_cast<float>(pv.pv_energy_total));
    if (this->dc_temperature_[i]) this->dc_temperature_[i]->publish_state(pv.pv_temp / 10.0f);

    // Grid-side (AC) readings are repeated identically on every PvDataMO
    // entry for a single inverter -- publish once, from the first channel.
    if (i == 0) {
      if (this->ac_voltage_) this->ac_voltage_->publish_state(pv.grid_vol / 10.0f);
      if (this->ac_current_) this->ac_current_->publish_state(pv.grid_i / 100.0f);
      if (this->ac_power_) this->ac_power_->publish_state(pv.grid_p / 10.0f);
      if (this->ac_frequency_) this->ac_frequency_->publish_state(pv.grid_freq / 100.0f);
      if (this->ac_reactive_power_) this->ac_reactive_power_->publish_state(pv.grid_q / 10.0f);
      if (this->ac_power_factor_) this->ac_power_factor_->publish_state(pv.grid_pf / 1000.0f);
      if (this->rssi_) this->rssi_->publish_state(static_cast<float>(pv.mi_signal));
    }
  }
#endif
}

void HMSWComponent::handle_real_data_new_(const RealDataNewReqDTO &data) {
  // Field scaling below (x10 for voltage/power/temperature, x100 for
  // current, x1000 for power_factor) is a HYPOTHESIS carried over from the
  // classic RealData/PvDataMO convention, NOT yet verified against a real
  // RealDataNew capture -- the .proto comments just say "Volts"/"Watts"/
  // etc. with no scale documented, same as they do for RealData (where the
  // x10/x100 convention was later confirmed empirically). If readings come
  // back 10x or 100x off on real hardware, adjust the divisors here.
  if (this->real_data_new_cp_ == 0) {
    uint8_t ap = (data.ap < 1) ? 1 : static_cast<uint8_t>(data.ap);
    if (ap > REAL_DATA_NEW_MAX_PAGES) ap = REAL_DATA_NEW_MAX_PAGES;
    this->real_data_new_ap_ = ap;
  }
  ESP_LOGD(TAG, "RealDataNew: page %u/%u, %u PV channel(s), %u SGS entr(y/ies), %u RP entr(y/ies)",
           (unsigned) (this->real_data_new_cp_ + 1), (unsigned) this->real_data_new_ap_,
           (unsigned) data.pv_data_count, (unsigned) data.sgs_data_count, (unsigned) data.rp_data_count);

#ifdef USE_SENSOR
  for (pb_size_t i = 0; i < data.pv_data_count && i < 4; i++) {
    const PvMO &pv = data.pv_data[i];
    if (this->dc_voltage_[i]) this->dc_voltage_[i]->publish_state(pv.voltage / 10.0f);
    if (this->dc_current_[i]) this->dc_current_[i]->publish_state(pv.current / 100.0f);
    if (this->dc_power_[i]) this->dc_power_[i]->publish_state(pv.power / 10.0f);
    if (this->dc_energy_total_[i]) this->dc_energy_total_[i]->publish_state(static_cast<float>(pv.energy_total));
    if (this->dc_energy_daily_[i]) this->dc_energy_daily_[i]->publish_state(static_cast<float>(pv.energy_daily));
    if (pv.error_code != 0) {
      ESP_LOGV(TAG, "PV channel %u reported error_code=%d", (unsigned) i, (int) pv.error_code);
    }
  }

  // Inverter-level AC/diagnostic block. HMS-XXXXW is single-phase, so this
  // targets sgs_data[0] (SGSMO -- "single grid-tied system", by analogy
  // with tgs_data's three-phase TGSMO); tgs_data/rsd_data are read from the
  // .proto for completeness but not wired up here since HMS-XXXXW doesn't
  // populate them. Reuses the SAME ac_*/dc_temperature_ sensor pointers as
  // classic RealData -- physically the same quantities -- so `ac:`/
  // `dc_channels: .../temperature` config is shared between both data
  // sources; only `energy_daily`/diagnostics/power_limit are RealDataNew-only.
  if (data.sgs_data_count > 0) {
    const SGSMO &sgs = data.sgs_data[0];
    if (this->ac_voltage_) this->ac_voltage_->publish_state(sgs.voltage / 10.0f);
    if (this->ac_frequency_) this->ac_frequency_->publish_state(sgs.frequency / 100.0f);
    if (this->ac_power_) this->ac_power_->publish_state(sgs.active_power / 10.0f);
    if (this->ac_reactive_power_) this->ac_reactive_power_->publish_state(sgs.reactive_power / 10.0f);
    if (this->ac_current_) this->ac_current_->publish_state(sgs.current / 100.0f);
    if (this->ac_power_factor_) this->ac_power_factor_->publish_state(sgs.power_factor / 1000.0f);
    if (this->dc_temperature_[0]) this->dc_temperature_[0]->publish_state(sgs.temperature / 10.0f);
    if (this->power_limit_readback_) this->power_limit_readback_->publish_state(sgs.power_limit / 10.0f);
    if (this->warning_number_) this->warning_number_->publish_state(static_cast<float>(sgs.warning_number));
    if (this->link_status_) this->link_status_->publish_state(static_cast<float>(sgs.link_status));
    ESP_LOGV(TAG, "RealDataNew crc_checksum=%d (log-only, not an entity -- see README.md)",
             (int) sgs.crc_checksum);
#ifdef USE_TEXT_SENSOR
    if (this->firmware_version_) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%d", (int) sgs.firmware_version);
      this->firmware_version_->publish_state(buf);
    }
#endif
  } else if (data.rp_data_count > 0 && this->link_status_) {
    // No sgs_data on this page (e.g. a page that only carries rp_data) --
    // fall back to the per-radio link_status from RpMO so the sensor still
    // gets *something* meaningful.
    this->link_status_->publish_state(static_cast<float>(data.rp_data[0].link_status));
  }
#endif

  uint8_t next_cp = this->real_data_new_cp_ + 1;
  if (next_cp < this->real_data_new_ap_) {
    this->real_data_new_cp_ = next_cp;
    this->real_data_new_pending_more_ = true;
  } else {
    this->real_data_new_cp_ = 0;
  }
}

void HMSWComponent::handle_command_response_(const CommandReqDTO &data) {
  if (data.err_code == 0) {
    ESP_LOGI(TAG, "Power limit command acknowledged (err_code=0)");
  } else {
    ESP_LOGW(TAG, "Power limit command reported err_code=%d", (int) data.err_code);
  }
}

void HMSWComponent::publish_reachable_(bool reachable) {
#ifdef USE_BINARY_SENSOR
  if (this->reachable_sensor_) this->reachable_sensor_->publish_state(reachable);
#endif
}

void HMSWComponent::loop() {
  switch (this->state_) {
    case ConnState::CONNECTING:
      this->handle_connecting_();
      return;
    case ConnState::SENDING:
      this->handle_sending_();
      return;
    case ConnState::RECEIVING:
      this->handle_receiving_();
      return;
    case ConnState::IDLE:
    default:
      break;
  }

  // Nothing in flight -- a pending power-limit command has priority over
  // everything else, sent as soon as the radio/connection is free instead
  // of waiting for the next poll_interval slot (same convention as
  // hm:/hms:). Next, an in-progress RealDataNew pagination continues
  // (see handle_real_data_new_()) rather than waiting for the next
  // poll_interval slot -- all pages belong to the same logical poll.
  // Otherwise realtime data is due next; the heartbeat is only sent if
  // neither is due, purely to exercise the connection between two
  // poll_interval slots on long intervals (matches async_heartbeat() in
  // the Python client -- see README.md for why this component does NOT
  // keep the TCP connection open between requests).
  if (this->power_limit_pending_) {
    this->power_limit_pending_ = false;
    this->start_request_(RequestKind::POWER_LIMIT);
    return;
  }

  if (this->real_data_new_pending_more_) {
    this->real_data_new_pending_more_ = false;
    this->start_request_(RequestKind::REAL_DATA_NEW);
    return;
  }

  uint32_t now = millis();
  if (now - this->last_poll_ >= this->poll_interval_ms_) {
    this->last_poll_ = now;
    if (this->use_real_data_new_) {
      // Fresh top-level poll: always start pagination from page 0, even if
      // a previous cycle was aborted mid-pagination (timeout, etc.) and
      // left real_data_new_cp_ non-zero.
      this->real_data_new_cp_ = 0;
      this->start_request_(RequestKind::REAL_DATA_NEW);
    } else {
      this->start_request_(RequestKind::REAL_DATA);
    }
  } else if (now - this->last_heartbeat_ >= this->heartbeat_interval_ms_) {
    this->last_heartbeat_ = now;
    this->start_request_(RequestKind::HEARTBEAT);
  }
}

void HMSWComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HMSW:");
  ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Poll interval: %ums", (unsigned) this->poll_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Heartbeat interval: %ums", (unsigned) this->heartbeat_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Request timeout: %ums", (unsigned) this->request_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Data source: %s", this->use_real_data_new_ ? "RealDataNew (0xA3 0x11)" : "RealData (0xA3 0x03)");
}

}  // namespace hmsw
}  // namespace esphome
