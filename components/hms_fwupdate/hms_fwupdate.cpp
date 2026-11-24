// SPDX-License-Identifier: GPL-2.0-or-later
#include "hms_fwupdate.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hms_fwupdate {

static const char *const TAG = "hms_fwupdate";

static const uint32_t STREAM_STALL_MS = 15000;  // no data from the HTTP server for this long -> abort
static const uint32_t ENTER_TIMEOUT_MS = 10000;  // the hub never became free
static const uint32_t SCAN_BUDGET_MS = 8;        // max time spent per loop() tick while verifying the file
static const uint32_t FETCH_BUDGET_MS = 4;

// Same addressing as hms.cpp (serial_to_packet_id): the low 32 bits of the serial, big endian.
static void serial_to_id(uint8_t out[4], uint64_t serial) {
  out[0] = static_cast<uint8_t>(serial >> 24);
  out[1] = static_cast<uint8_t>(serial >> 16);
  out[2] = static_cast<uint8_t>(serial >> 8);
  out[3] = static_cast<uint8_t>(serial >> 0);
}

// ---------------------------------------------------------------------------
// setup / publishing
// ---------------------------------------------------------------------------
void HMSFwUpdateComponent::setup() {
  this->disable_loop();  // nothing to do until start() is called
  this->set_state_("idle");
  this->set_result_("none");
  this->publish_progress_(0.0f, true);
  this->publish_running_();
#ifdef USE_SWITCH
  if (this->armed_switch_ != nullptr) this->armed_switch_->publish_state(false);
#endif
#ifdef USE_SELECT
  if (this->firmware_select_ != nullptr && !this->firmwares_.empty())
    this->firmware_select_->publish_state(this->firmwares_[0].name);
#endif
#ifdef USE_SENSOR
  if (this->link_quality_sensor_ != nullptr)
    this->set_interval("link_quality", 10000, [this]() { this->publish_link_quality_(); });
#endif
}

void HMSFwUpdateComponent::set_state_(const char *state) {
#ifdef USE_TEXT_SENSOR
  if (this->state_sensor_ != nullptr) this->state_sensor_->publish_state(state);
#endif
  ESP_LOGD(TAG, "State: %s", state);
}

void HMSFwUpdateComponent::set_result_(const std::string &result) {
#ifdef USE_TEXT_SENSOR
  if (this->result_sensor_ != nullptr) this->result_sensor_->publish_state(result);
#endif
  for (auto &cb : this->result_cbs_) cb(result);
}

void HMSFwUpdateComponent::publish_progress_(float pct, bool force) {
  if (!force && this->last_progress_ >= 0.0f && pct - this->last_progress_ < 1.0f) return;  // 1 % steps
  this->last_progress_ = pct;
#ifdef USE_SENSOR
  if (this->progress_sensor_ != nullptr) this->progress_sensor_->publish_state(pct);
#endif
  for (auto &cb : this->progress_cbs_) cb(pct);
}

void HMSFwUpdateComponent::publish_link_quality_() {
#ifdef USE_SENSOR
  if (this->link_quality_sensor_ == nullptr || this->hub_ == nullptr) return;
  if (this->hub_->get_link_samples() == 0) return;
  this->link_quality_sensor_->publish_state(this->hub_->get_link_quality(this->link_quality_window_));
#endif
}

void HMSFwUpdateComponent::publish_running_() {
#ifdef USE_BINARY_SENSOR
  if (this->running_sensor_ != nullptr) this->running_sensor_->publish_state(this->phase_ != Phase::IDLE);
#endif
}

void HMSFwUpdateComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HMS firmware update:");
  ESP_LOGCONFIG(TAG, "  Firmwares:");
  for (const auto &fw : this->firmwares_) {
    ESP_LOGCONFIG(TAG, "    - %s (%s)%s", fw.name.c_str(), fw.url.c_str(), fw.sha256.empty() ? "" : " [sha256 pinned]");
  }
  ESP_LOGCONFIG(TAG, "  Minimum link quality: %u%% over the last %u polls", this->min_link_quality_,
                this->link_quality_window_);
  ESP_LOGCONFIG(TAG, "  Require arming: %s (armed for %ums)", YESNO(this->require_armed_),
                static_cast<unsigned int>(this->armed_timeout_ms_));
  ESP_LOGCONFIG(TAG, "  Max attempts per row: %u", this->max_row_attempts_);
  ESP_LOGCONFIG(TAG, "  Strict Intel HEX checks: %s", YESNO(this->strict_hex_));
  if (this->dry_run_) {
    ESP_LOGCONFIG(TAG, "  DRY RUN: the file is verified but NOTHING is sent to the inverter");
  } else {
    ESP_LOGW(TAG, "  dry_run is OFF: 'start' WILL flash the inverter. An interrupted update can leave it unusable "
                  "until a new update succeeds.");
  }
}

// ---------------------------------------------------------------------------
// control
// ---------------------------------------------------------------------------
void HMSFwUpdateComponent::set_armed(bool armed) {
  this->armed_ = armed;
  this->cancel_timeout("disarm");
  if (armed) {
    this->set_timeout("disarm", this->armed_timeout_ms_, [this]() {
      ESP_LOGI(TAG, "Arming expired");
      this->set_armed(false);
    });
    ESP_LOGW(TAG, "ARMED for %us: press 'start' to begin", static_cast<unsigned int>(this->armed_timeout_ms_ / 1000));
  }
#ifdef USE_SWITCH
  if (this->armed_switch_ != nullptr) this->armed_switch_->publish_state(armed);
#endif
}

bool HMSFwUpdateComponent::select_firmware(const std::string &name) {
  if (this->phase_ != Phase::IDLE) {
    ESP_LOGW(TAG, "Cannot change the firmware while an update is running");
    return false;
  }
  for (size_t i = 0; i < this->firmwares_.size(); i++) {
    if (this->firmwares_[i].name == name) {
      this->selected_ = i;
      ESP_LOGI(TAG, "Selected firmware: %s", name.c_str());
      return true;
    }
  }
  return false;
}

void HMSFwUpdateComponent::abort() {
  if (this->phase_ == Phase::IDLE) return;
  ESP_LOGW(TAG, "Abort requested");
  this->abort_requested_ = true;
}

void HMSFwUpdateComponent::start() {
  if (this->phase_ != Phase::IDLE) {
    ESP_LOGW(TAG, "Start ignored: an update is already running");
    return;
  }
  if (this->hub_ == nullptr || this->http_client_ == nullptr || this->firmwares_.empty()) {
    this->finish_(Outcome::FAILED, "component not configured");
    return;
  }
  if (this->require_armed_ && !this->dry_run_ && !this->armed_) {
    this->finish_(Outcome::FAILED, "not armed (turn the armed switch on, then press start within %us)",
                  static_cast<unsigned int>(this->armed_timeout_ms_ / 1000));
    return;
  }
  if (this->armed_) this->set_armed(false);  // arming is one-shot

  this->abort_requested_ = false;
  this->rows_done_ = 0;
  this->stat_last_pkt_resends_ = 0;
  this->stat_row_resends_ = 0;
  this->last_progress_ = -1.0f;
  this->publish_progress_(0.0f, true);
  this->t_start_ = millis();

  const FirmwareEntry &fw = this->firmwares_[this->selected_];
  ESP_LOGI(TAG, "Starting%s: %s", this->dry_run_ ? " (DRY RUN)" : "", fw.name.c_str());

  if (!this->preflight_()) return;
  if (!this->begin_scan_()) return;

  this->phase_ = Phase::SCAN;
  this->set_result_("running");
  this->set_state_("scanning");
  this->publish_running_();
  this->enable_loop();
}

bool HMSFwUpdateComponent::preflight_() {
  const uint64_t sn = this->hub_->get_inverter_serial();
  this->rule_ = fw_find_rule(sn);
  if (this->rule_ == nullptr) {
    this->finish_(Outcome::FAILED, "inverter model not supported for firmware update (serial prefix %04X)",
                  static_cast<unsigned int>((sn >> 32) & 0xFFFF));
    return false;
  }
  serial_to_id(this->inv_id_, sn);
  serial_to_id(this->dtu_id_, this->hub_->get_dtu_serial());

  const uint8_t samples = this->hub_->get_link_samples();
  const uint8_t quality = this->hub_->get_link_quality(this->link_quality_window_);
  ESP_LOGI(TAG, "Link: reachable=%s, quality %u%% over %u/%u polls (minimum %u%%)", YESNO(this->hub_->is_reachable()),
           quality, samples < this->link_quality_window_ ? samples : this->link_quality_window_,
           this->link_quality_window_, this->min_link_quality_);

  if (this->dry_run_) return true;  // nothing is transmitted in a dry run

  if (!this->hub_->is_reachable()) {
    this->finish_(Outcome::FAILED, "inverter not reachable");
    return false;
  }
  if (samples < this->link_quality_window_) {
    this->finish_(Outcome::FAILED, "link history too short (%u/%u polls) -- wait a little and retry", samples,
                  this->link_quality_window_);
    return false;
  }
  if (quality < this->min_link_quality_) {
    this->finish_(Outcome::FAILED, "link quality too low (%u%% < %u%%)", quality, this->min_link_quality_);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// HTTP stream / line reader
// ---------------------------------------------------------------------------
bool HMSFwUpdateComponent::open_stream_() {
  const FirmwareEntry &fw = this->firmwares_[this->selected_];
  this->close_stream_();
  this->http_ = this->http_client_->get(fw.url);
  if (this->http_ == nullptr) {
    this->finish_(Outcome::FAILED, "cannot reach %s", fw.url.c_str());
    return false;
  }
  if (this->http_->status_code != 200) {
    const int code = this->http_->status_code;
    this->finish_(Outcome::FAILED, "HTTP status %d for %s", code, fw.url.c_str());
    return false;
  }
  this->rpos_ = 0;
  this->rlen_ = 0;
  this->stream_eof_ = false;
  this->stream_wait_since_ = 0;
  return true;
}

void HMSFwUpdateComponent::close_stream_() {
  if (this->http_ != nullptr) {
    this->http_->end();
    this->http_.reset();
  }
  this->rpos_ = 0;
  this->rlen_ = 0;
  this->stream_eof_ = false;
}

HMSFwUpdateComponent::LineStatus HMSFwUpdateComponent::read_line_(bool hash) {
  for (;;) {
    // 1. a complete line is already buffered?
    uint8_t *start = this->rbuf_ + this->rpos_;
    uint8_t *nl = static_cast<uint8_t *>(memchr(start, '\n', this->rlen_ - this->rpos_));
    if (nl != nullptr) {
      const size_t n = static_cast<size_t>(nl - start);
      if (n >= sizeof(this->line_)) return LineStatus::ERROR;
      memcpy(this->line_, start, n);
      this->line_[n] = '\0';
      this->line_len_ = n;
      this->rpos_ += n + 1;
      return LineStatus::LINE;
    }
    // 2. compact, then read more
    if (this->rpos_ > 0) {
      memmove(this->rbuf_, this->rbuf_ + this->rpos_, this->rlen_ - this->rpos_);
      this->rlen_ -= this->rpos_;
      this->rpos_ = 0;
    }
    if (this->stream_eof_) {  // last line without a trailing newline
      if (this->rlen_ == 0) return LineStatus::END;
      if (this->rlen_ >= sizeof(this->line_)) return LineStatus::ERROR;
      memcpy(this->line_, this->rbuf_, this->rlen_);
      this->line_[this->rlen_] = '\0';
      this->line_len_ = this->rlen_;
      this->rlen_ = 0;
      return LineStatus::LINE;
    }
    if (this->rlen_ >= sizeof(this->rbuf_)) return LineStatus::ERROR;  // a line longer than the buffer

    const int n = this->http_->read(this->rbuf_ + this->rlen_, sizeof(this->rbuf_) - this->rlen_);
    if (n < 0) return LineStatus::ERROR;
    if (n == 0) {
      if (this->http_->is_read_complete()) {
        this->stream_eof_ = true;
        continue;
      }
      return LineStatus::PENDING;
    }
    if (hash && this->hasher_ != nullptr) this->hasher_->add(this->rbuf_ + this->rlen_, static_cast<size_t>(n));
    this->rlen_ += static_cast<size_t>(n);
  }
}

// ---------------------------------------------------------------------------
// Pass 1: verify the whole file before touching the radio
// ---------------------------------------------------------------------------
bool HMSFwUpdateComponent::begin_scan_() {
  this->lines_seen_ = 0;
  this->rows_total_ = 0;
  this->packets_total_ = 0;
  this->eof_seen_ = false;
  this->first_row_checked_ = false;
  this->dry_run_logged_ = 0;
  this->hasher_ = std::make_unique<sha256::SHA256>();
  this->hasher_->init();
  return this->open_stream_();
}

void HMSFwUpdateComponent::step_scan_() {
  if (this->abort_requested_) {
    this->finish_(Outcome::ABORTED, "aborted during verification (nothing was sent to the inverter)");
    return;
  }
  const uint32_t t0 = millis();
  while (millis() - t0 < SCAN_BUDGET_MS) {
    const LineStatus s = this->read_line_(true);
    if (s == LineStatus::PENDING) {
      if (this->stream_wait_since_ == 0) {
        this->stream_wait_since_ = millis();
      } else if (millis() - this->stream_wait_since_ > STREAM_STALL_MS) {
        this->finish_(Outcome::FAILED, "download stalled");
      }
      return;
    }
    this->stream_wait_since_ = 0;
    if (s == LineStatus::ERROR) {
      this->finish_(Outcome::FAILED, "download error (or a line longer than %u bytes)",
                    static_cast<unsigned int>(sizeof(this->rbuf_)));
      return;
    }
    if (s == LineStatus::END) {
      this->end_scan_();
      return;
    }

    this->lines_seen_++;
    size_t n = 0;
    const HexResult r = fw_decode_hex_line(this->line_, this->line_len_, this->row_, sizeof(this->row_), n,
                                           this->strict_hex_);
    if (r == HexResult::SKIP) continue;
    if (r == HexResult::ERROR) {
      this->finish_(Outcome::FAILED, "invalid Intel HEX data at line %u%s", static_cast<unsigned int>(this->lines_seen_),
                    this->strict_hex_ ? " (checksum or length)" : "");
      return;
    }
    if (this->eof_seen_) continue;  // anything after the EOF record is ignored (it is still hashed)

    if (!this->first_row_checked_) {
      if (r != HexResult::DATA || n < 8) {
        this->finish_(Outcome::FAILED, "unrecognized identity row (first row of the file)");
        return;
      }
      if (!fw_identity_matches(*this->rule_, this->row_, n)) {
        this->finish_(Outcome::FAILED, "this file is not for this inverter (serial prefix %04X, identity %02X %02X %02X %02X)",
                      static_cast<unsigned int>((this->hub_->get_inverter_serial() >> 32) & 0xFFFF), this->row_[4],
                      this->row_[5], this->row_[6], this->row_[7]);
        return;
      }
      this->first_row_checked_ = true;
    }

    const size_t np = fw_packetize_row(this->row_, n, this->packets_, FW_MAX_PACKETS);
    if (np == 0) {
      this->finish_(Outcome::FAILED, "row too long at line %u", static_cast<unsigned int>(this->lines_seen_));
      return;
    }
    this->rows_total_++;
    this->packets_total_ += static_cast<uint32_t>(np);

    // Dry run: show the frames of the first rows and of the EOF row, in the reference implementation's log
    // layout, so they can be compared byte for byte with a real OpenDTU (FirmwareUpdate_HM_IV) capture.
    if (this->dry_run_ && (this->rows_total_ <= 3 || r == HexResult::END_OF_FILE)) {
      for (size_t i = 0; i < np; i++) {
        uint8_t frame[32];
        const uint8_t len = fw_build_frame(this->inv_id_, this->dtu_id_, this->packets_[i], this->row_, n, frame);
        char hex[3 * 32 + 1];
        fw_hex_dump(frame, len, hex, sizeof(hex));
        ESP_LOGI(TAG, "row %u pkt %u/%u TX FirmwareData --> %s", static_cast<unsigned int>(this->rows_total_),
                 static_cast<unsigned int>(i + 1), static_cast<unsigned int>(np), hex);
      }
    }
    if (r == HexResult::END_OF_FILE) this->eof_seen_ = true;
  }
}

void HMSFwUpdateComponent::end_scan_() {
  const FirmwareEntry &fw = this->firmwares_[this->selected_];
  if (this->http_ != nullptr && this->http_->content_length > 0 &&
      this->http_->get_bytes_read() < this->http_->content_length) {
    this->finish_(Outcome::FAILED, "download truncated (%u of %u bytes)",
                  static_cast<unsigned int>(this->http_->get_bytes_read()),
                  static_cast<unsigned int>(this->http_->content_length));
    return;
  }
  if (!this->eof_seen_ || this->rows_total_ == 0) {
    this->finish_(Outcome::FAILED, "no end-of-file record in the file");
    return;
  }

  this->hasher_->calculate();
  char hex[65];
  this->hasher_->get_hex(hex);
  ESP_LOGI(TAG, "File verified: %u rows, %u packets, SHA-256 %s", static_cast<unsigned int>(this->rows_total_),
           static_cast<unsigned int>(this->packets_total_), hex);
  if (!fw.sha256.empty() && !this->hasher_->equals_hex(fw.sha256.c_str())) {
    this->finish_(Outcome::FAILED, "SHA-256 mismatch (file %s, expected %s)", hex, fw.sha256.c_str());
    return;
  }
  if (fw.sha256.empty()) ESP_LOGI(TAG, "Tip: pin it with `sha256: \"%s\"` in the firmwares list", hex);

  this->close_stream_();
  this->hasher_.reset();

  if (this->dry_run_) {
    this->finish_(Outcome::DRY_RUN_OK, "%u rows, %u packets -- nothing was sent",
                  static_cast<unsigned int>(this->rows_total_), static_cast<unsigned int>(this->packets_total_));
    return;
  }

  // Second download: this time the rows are sent as they arrive.
  if (!this->open_stream_()) return;
  this->phase_ = Phase::ENTER;
  this->enter_since_ = millis();
  this->set_state_("starting");
}

// ---------------------------------------------------------------------------
// Pass 2: send
// ---------------------------------------------------------------------------
void HMSFwUpdateComponent::step_enter_() {
  if (this->abort_requested_) {
    this->finish_(Outcome::ABORTED, "aborted before sending (nothing was sent to the inverter)");
    return;
  }
  if (millis() - this->enter_since_ > ENTER_TIMEOUT_MS) {
    this->finish_(Outcome::FAILED, "the radio stayed busy, could not start");
    return;
  }
  // The link may have degraded during the download: check again right before touching the radio.
  if (!this->hub_->is_reachable() ||
      this->hub_->get_link_quality(this->link_quality_window_) < this->min_link_quality_) {
    this->finish_(Outcome::FAILED, "link quality dropped during verification (%u%%)",
                  this->hub_->get_link_quality(this->link_quality_window_));
    return;
  }
  if (!this->hub_->fw_enter([this](const uint8_t *frame, uint8_t len) { this->on_frame_(frame, len); })) {
    return;  // exchange in flight: retry on the next tick
  }
  this->in_maintenance_ = true;
  this->high_freq_.start();  // the 30 ms packet spacing needs a fast loop
  this->rows_done_ = 0;
  this->tx_done_ms_ = millis();
  this->awaiting_tx_done_ = false;
  this->phase_ = Phase::ROW_FETCH;
  this->set_state_("sending");
  ESP_LOGW(TAG, "Sending %u rows (%u packets). Do not power-cycle the inverter or the ESP32.",
           static_cast<unsigned int>(this->rows_total_), static_cast<unsigned int>(this->packets_total_));
}

void HMSFwUpdateComponent::loop() {
  switch (this->phase_) {
    case Phase::SCAN:
      this->step_scan_();
      break;
    case Phase::ENTER:
      this->step_enter_();
      break;
    case Phase::ROW_FETCH:
    case Phase::ROW_TX:
    case Phase::ROW_WAIT_ACK:
      this->step_send_();
      break;
    default:
      break;
  }
}

void HMSFwUpdateComponent::step_send_() {
  if (this->abort_requested_) {
    this->finish_(Outcome::ABORTED, "aborted after %u of %u rows", static_cast<unsigned int>(this->rows_done_),
                  static_cast<unsigned int>(this->rows_total_));
    return;
  }
  // A packet is on the air: nothing else happens until the hub reports the end of the transmission.
  if (this->awaiting_tx_done_) {
    if (this->hub_->fw_tx_busy()) return;
    this->awaiting_tx_done_ = false;
    this->tx_done_ms_ = millis();
    if (this->phase_ == Phase::ROW_WAIT_ACK) this->ack_deadline_ = this->tx_done_ms_ + FW_ACK_TIMEOUT_MS;
  }
  switch (this->phase_) {
    case Phase::ROW_FETCH:
      this->fetch_row_();
      break;
    case Phase::ROW_TX:
      this->tx_next_packet_();
      break;
    case Phase::ROW_WAIT_ACK:
      this->check_ack_();
      break;
    default:
      break;
  }
}

void HMSFwUpdateComponent::fetch_row_() {
  const uint32_t t0 = millis();
  while (millis() - t0 < FETCH_BUDGET_MS) {
    const LineStatus s = this->read_line_(false);
    if (s == LineStatus::PENDING) {
      if (this->stream_wait_since_ == 0) {
        this->stream_wait_since_ = millis();
      } else if (millis() - this->stream_wait_since_ > STREAM_STALL_MS) {
        this->finish_(Outcome::FAILED, "download stalled at row %u", static_cast<unsigned int>(this->rows_done_ + 1));
      }
      return;
    }
    this->stream_wait_since_ = 0;
    if (s == LineStatus::ERROR) {
      this->finish_(Outcome::FAILED, "download error at row %u", static_cast<unsigned int>(this->rows_done_ + 1));
      return;
    }
    if (s == LineStatus::END) {
      this->finish_(Outcome::FAILED, "the file ended before its end-of-file record");
      return;
    }

    size_t n = 0;
    const HexResult r = fw_decode_hex_line(this->line_, this->line_len_, this->row_, sizeof(this->row_), n,
                                           this->strict_hex_);
    if (r == HexResult::SKIP) continue;
    if (r == HexResult::ERROR) {
      this->finish_(Outcome::FAILED, "invalid Intel HEX data at row %u (the file changed since verification?)",
                    static_cast<unsigned int>(this->rows_done_ + 1));
      return;
    }
    if (this->rows_done_ == 0 && (r != HexResult::DATA || !fw_identity_matches(*this->rule_, this->row_, n))) {
      this->finish_(Outcome::FAILED, "the first row no longer matches this inverter (the file changed?)");
      return;
    }

    this->row_len_ = n;
    this->n_packets_ = fw_packetize_row(this->row_, n, this->packets_, FW_MAX_PACKETS);
    if (this->n_packets_ == 0) {
      this->finish_(Outcome::FAILED, "row too long at row %u", static_cast<unsigned int>(this->rows_done_ + 1));
      return;
    }
    this->pkt_idx_ = 0;
    this->attempt_ = 1;
    this->last_resends_ = 0;
    this->acked_ = false;
    this->phase_ = Phase::ROW_TX;
    return;
  }
}

bool HMSFwUpdateComponent::send_packet_(size_t index) {
  uint8_t frame[32];
  const uint8_t len = fw_build_frame(this->inv_id_, this->dtu_id_, this->packets_[index], this->row_, this->row_len_,
                                     frame);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  char hex[3 * 32 + 1];
  fw_hex_dump(frame, len, hex, sizeof(hex));
  ESP_LOGVV(TAG, "TX row %u pkt %u/%u attempt %u --> %s", static_cast<unsigned int>(this->rows_done_ + 1),
            static_cast<unsigned int>(index + 1), static_cast<unsigned int>(this->n_packets_), this->attempt_, hex);
#endif
  if (!this->hub_->fw_send_frame(frame, len)) {
    ESP_LOGW(TAG, "Radio refused the packet (busy), retrying");
    return false;
  }
  this->awaiting_tx_done_ = true;
  return true;
}

void HMSFwUpdateComponent::tx_next_packet_() {
  if (this->pkt_idx_ >= this->n_packets_) {
    this->phase_ = Phase::ROW_WAIT_ACK;
    return;
  }
  // The inverter expects ~30 ms between packets of a row (the reference implementation's Rx window).
  if (millis() - this->tx_done_ms_ < FW_GAP_MS) return;

  const bool is_last = fw_packet_expects_ack(this->packets_[this->pkt_idx_]);
  if (!this->send_packet_(this->pkt_idx_)) return;
  this->pkt_idx_++;
  if (is_last) this->phase_ = Phase::ROW_WAIT_ACK;  // the ack deadline starts when the transmission ends
}

void HMSFwUpdateComponent::on_frame_(const uint8_t *frame, uint8_t len) {
  if (this->phase_ != Phase::ROW_WAIT_ACK) return;
  if (fw_is_row_ack(frame, len, this->row_, this->row_len_)) {
    this->acked_ = true;
  } else {
    char hex[3 * 32 + 1];
    fw_hex_dump(frame, len, hex, sizeof(hex));
    ESP_LOGV(TAG, "Unexpected frame while waiting for the ack: %s", hex);
  }
}

void HMSFwUpdateComponent::check_ack_() {
  if (this->acked_) {
    this->acked_ = false;
    this->row_done_();
    return;
  }
  if (static_cast<int32_t>(millis() - this->ack_deadline_) < 0) return;

  // No acknowledgement. Same two nested levels as the reference implementation:
  //  1. re-send the LAST packet of the row (it carries the CRC) a few times,
  //  2. then re-send the whole row, up to max_row_attempts times.
  const uint8_t resends = fw_ack_resends(this->row_[3]);
  if (this->last_resends_ < resends) {
    this->last_resends_++;
    this->stat_last_pkt_resends_++;
    this->send_packet_(this->n_packets_ - 1);
    return;
  }
  if (this->attempt_ < this->max_row_attempts_) {
    this->attempt_++;
    this->last_resends_ = 0;
    this->pkt_idx_ = 0;
    this->stat_row_resends_++;
    ESP_LOGW(TAG, "Row %u not acknowledged, re-sending the whole row (attempt %u/%u)",
             static_cast<unsigned int>(this->rows_done_ + 1), this->attempt_, this->max_row_attempts_);
    this->phase_ = Phase::ROW_TX;
    return;
  }
  this->finish_(Outcome::FAILED, "row %u (address %02X%02X, type %02X) not acknowledged after %u attempts",
                static_cast<unsigned int>(this->rows_done_ + 1), this->row_[1], this->row_[2], this->row_[3],
                this->max_row_attempts_);
}

void HMSFwUpdateComponent::row_done_() {
  this->rows_done_++;
  const float pct = this->rows_total_ ? 100.0f * static_cast<float>(this->rows_done_) / static_cast<float>(this->rows_total_)
                                      : 0.0f;
  this->publish_progress_(pct);

  if (this->row_[3] == 0x01) {  // EOF row acknowledged: the inverter commits / verifies the image
    if (this->rows_done_ != this->rows_total_) {
      ESP_LOGW(TAG, "EOF acknowledged after %u rows, %u were expected", static_cast<unsigned int>(this->rows_done_),
               static_cast<unsigned int>(this->rows_total_));
    }
    this->finish_(Outcome::SUCCESS, "%u rows in %us (%u row re-sends, %u last-packet re-sends)",
                  static_cast<unsigned int>(this->rows_done_),
                  static_cast<unsigned int>((millis() - this->t_start_) / 1000),
                  static_cast<unsigned int>(this->stat_row_resends_),
                  static_cast<unsigned int>(this->stat_last_pkt_resends_));
    return;
  }
  this->phase_ = Phase::ROW_FETCH;
}

// ---------------------------------------------------------------------------
// end of a run (success, failure, abort, dry run)
// ---------------------------------------------------------------------------
void HMSFwUpdateComponent::finish_(Outcome outcome, const char *fmt, ...) {
  char msg[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);

  const bool was_sending = this->in_maintenance_;
  this->close_stream_();
  this->hasher_.reset();
  if (this->in_maintenance_) {
    this->hub_->fw_leave();
    this->in_maintenance_ = false;
  }
  this->high_freq_.stop();
  this->phase_ = Phase::IDLE;
  this->abort_requested_ = false;
  this->awaiting_tx_done_ = false;

  const char *prefix = "failed";
  const char *state = "failed";
  switch (outcome) {
    case Outcome::SUCCESS:
      prefix = "success";
      state = "done";
      break;
    case Outcome::DRY_RUN_OK:
      prefix = "dry_run_ok";
      state = "dry_run_done";
      break;
    case Outcome::ABORTED:
      prefix = "aborted";
      state = "aborted";
      break;
    default:
      break;
  }
  if (outcome == Outcome::FAILED) {
    ESP_LOGE(TAG, "Update failed: %s", msg);
  } else {
    ESP_LOGI(TAG, "Update %s: %s", prefix, msg);
  }
  if (was_sending && this->rows_done_ > 0 && outcome != Outcome::SUCCESS) {
    ESP_LOGW(TAG, "The inverter received %u rows and may be waiting in its bootloader. Run the update again "
                  "(same file) until it succeeds.",
             static_cast<unsigned int>(this->rows_done_));
  }
  if (outcome == Outcome::SUCCESS) {
    ESP_LOGW(TAG, "The inverter reboots with the new firmware; it may take a few minutes to become reachable again.");
  }

  this->set_state_(state);
  this->set_result_(std::string(prefix) + ": " + msg);
  if (outcome == Outcome::SUCCESS) this->publish_progress_(100.0f, true);
  this->publish_running_();
  this->disable_loop();
}

}  // namespace hms_fwupdate
}  // namespace esphome
