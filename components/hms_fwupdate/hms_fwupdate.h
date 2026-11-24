// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/hms/hms.h"
#include "esphome/components/http_request/http_request.h"
#include "esphome/components/sha256/sha256.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

#include "fwupdate_core.h"

namespace esphome {
namespace hms_fwupdate {

/// One firmware choice from the YAML `firmwares:` list.
struct FirmwareEntry {
  std::string name;
  std::string url;
  std::string sha256;  // 64 hex characters, or empty = not pinned
};

enum class Phase : uint8_t {
  IDLE = 0,
  SCAN,          // pass 1: download + verify the whole file (hash, Intel HEX checksums, identity row)
  ENTER,         // wait for the hub to be free, then take the radio
  ROW_FETCH,     // pass 2: read the next row from the stream
  ROW_TX,        // send the packets of the current row
  ROW_WAIT_ACK,  // wait for the inverter to acknowledge the row
};

enum class Outcome : uint8_t { SUCCESS, DRY_RUN_OK, FAILED, ABORTED };

class HMSFwUpdateComponent : public Component {
 public:
  // --- configuration -------------------------------------------------------
  void set_hub(hms::HMSComponent *hub) { this->hub_ = hub; }
  void set_http(http_request::HttpRequestComponent *http) { this->http_client_ = http; }
  void add_firmware(const std::string &name, const std::string &url, const std::string &sha256) {
    this->firmwares_.push_back({name, url, sha256});
  }
  void set_min_link_quality(uint8_t pct) { this->min_link_quality_ = pct; }
  void set_link_quality_window(uint8_t polls) { this->link_quality_window_ = polls; }
  void set_require_armed(bool v) { this->require_armed_ = v; }
  void set_armed_timeout(uint32_t ms) { this->armed_timeout_ms_ = ms; }
  void set_dry_run(bool v) { this->dry_run_ = v; }
  void set_max_row_attempts(uint8_t n) { this->max_row_attempts_ = n; }
  void set_strict_hex(bool v) { this->strict_hex_ = v; }

#ifdef USE_SENSOR
  void set_progress_sensor(sensor::Sensor *s) { this->progress_sensor_ = s; }
  void set_link_quality_sensor(sensor::Sensor *s) { this->link_quality_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_state_text_sensor(text_sensor::TextSensor *s) { this->state_sensor_ = s; }
  void set_result_text_sensor(text_sensor::TextSensor *s) { this->result_sensor_ = s; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_running_binary_sensor(binary_sensor::BinarySensor *s) { this->running_sensor_ = s; }
#endif
#ifdef USE_SWITCH
  void set_armed_switch(switch_::Switch *s) { this->armed_switch_ = s; }
#endif
#ifdef USE_SELECT
  void set_firmware_select(select::Select *s) { this->firmware_select_ = s; }
#endif

  void add_on_progress_callback(std::function<void(float)> &&cb) { this->progress_cbs_.push_back(std::move(cb)); }
  void add_on_result_callback(std::function<void(const std::string &)> &&cb) {
    this->result_cbs_.push_back(std::move(cb));
  }

  // --- control (buttons, switch, select, automations) -----------------------
  void start();
  void abort();
  void set_armed(bool armed);
  bool is_armed() const { return this->armed_; }
  /// Returns false if no firmware has this name.
  bool select_firmware(const std::string &name);
  bool is_running() const { return this->phase_ != Phase::IDLE; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  // --- flow ---------------------------------------------------------------
  bool preflight_();
  bool begin_scan_();
  void step_scan_();
  void end_scan_();
  void step_enter_();
  void step_send_();
  void fetch_row_();
  void tx_next_packet_();
  bool send_packet_(size_t index);
  void check_ack_();
  void row_done_();
  void finish_(Outcome outcome, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
  void on_frame_(const uint8_t *frame, uint8_t len);

  // --- HTTP line reader ---------------------------------------------------
  enum class LineStatus : uint8_t { LINE, PENDING, END, ERROR };
  bool open_stream_();
  void close_stream_();
  LineStatus read_line_(bool hash);

  // --- publishing ---------------------------------------------------------
  void set_state_(const char *state);
  void set_result_(const std::string &result);
  void publish_progress_(float pct, bool force = false);
  void publish_link_quality_();
  void publish_running_();

  hms::HMSComponent *hub_{nullptr};
  http_request::HttpRequestComponent *http_client_{nullptr};
  std::vector<FirmwareEntry> firmwares_;
  size_t selected_{0};

  uint8_t min_link_quality_{90};
  uint8_t link_quality_window_{20};
  bool require_armed_{true};
  uint32_t armed_timeout_ms_{60000};
  bool dry_run_{true};
  uint8_t max_row_attempts_{10};
  bool strict_hex_{true};

  bool armed_{false};
  Phase phase_{Phase::IDLE};
  bool abort_requested_{false};
  bool in_maintenance_{false};
  HighFrequencyLoopRequester high_freq_;

  // inverter / DTU radio addresses
  uint8_t inv_id_[4]{};
  uint8_t dtu_id_[4]{};
  const FwSerialRule *rule_{nullptr};

  // stream
  std::shared_ptr<http_request::HttpContainer> http_;
  uint8_t rbuf_[1024]{};
  size_t rpos_{0};
  size_t rlen_{0};
  bool stream_eof_{false};
  char line_[560]{};
  size_t line_len_{0};
  uint32_t stream_wait_since_{0};
  std::unique_ptr<sha256::SHA256> hasher_;

  // scan results
  uint32_t lines_seen_{0};
  uint32_t rows_total_{0};
  uint32_t packets_total_{0};
  bool eof_seen_{false};
  bool first_row_checked_{false};
  uint32_t dry_run_logged_{0};

  // current row
  uint8_t row_[FW_MAX_ROW_LEN + 1]{};
  size_t row_len_{0};
  FwPacket packets_[FW_MAX_PACKETS]{};
  size_t n_packets_{0};
  size_t pkt_idx_{0};
  uint8_t attempt_{1};
  uint8_t last_resends_{0};
  volatile bool acked_{false};
  bool awaiting_tx_done_{false};
  uint32_t tx_done_ms_{0};
  uint32_t ack_deadline_{0};
  uint32_t rows_done_{0};

  // statistics
  uint32_t t_start_{0};
  uint32_t stat_last_pkt_resends_{0};
  uint32_t stat_row_resends_{0};
  uint32_t enter_since_{0};
  float last_progress_{-1.0f};

  // entities
#ifdef USE_SENSOR
  sensor::Sensor *progress_sensor_{nullptr};
  sensor::Sensor *link_quality_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *state_sensor_{nullptr};
  text_sensor::TextSensor *result_sensor_{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *running_sensor_{nullptr};
#endif
#ifdef USE_SWITCH
  switch_::Switch *armed_switch_{nullptr};
#endif
#ifdef USE_SELECT
  select::Select *firmware_select_{nullptr};
#endif

  std::vector<std::function<void(float)>> progress_cbs_;
  std::vector<std::function<void(const std::string &)>> result_cbs_;
};

// --- automation ---------------------------------------------------------------
class ProgressTrigger : public Trigger<float> {
 public:
  explicit ProgressTrigger(HMSFwUpdateComponent *parent) {
    parent->add_on_progress_callback([this](float pct) { this->trigger(pct); });
  }
};

class ResultTrigger : public Trigger<std::string> {
 public:
  explicit ResultTrigger(HMSFwUpdateComponent *parent) {
    parent->add_on_result_callback([this](const std::string &r) { this->trigger(r); });
  }
};

template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<HMSFwUpdateComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->start(); }
};

template<typename... Ts> class AbortAction : public Action<Ts...>, public Parented<HMSFwUpdateComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->abort(); }
};

template<typename... Ts> class ArmAction : public Action<Ts...>, public Parented<HMSFwUpdateComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->set_armed(true); }
};

}  // namespace hms_fwupdate
}  // namespace esphome
