#pragma once

#include <string>
#include <memory>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/socket/socket.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

// nanopb-generated messages for the Hoymiles "local TCP/Protobuf" protocol
// used by the newer HMS-XXXXW WiFi microinverters (integrated DTU, no
// external radio bridge). Reverse-engineered definitions ported from
// henkwiedig/Hoymiles-DTU-Proto and suaveolent/hoymiles-wifi (Python
// reference client) -- see README.md for the exact sources and the framing
// this component implements.
#include "RealData.pb.h"
#include "RealDataNew.pb.h"
#include "APPHeartbeatPB.pb.h"
#include "CommandPB.pb.h"
#include "AlarmData.pb.h"

namespace esphome {
namespace hmsw {

// ---------------------------------------------------------------------------
// Wire format (unencrypted, non-"extended" variant -- the common case for a
// local, unauthenticated connection; see README.md for the encrypted/
// extended variants this component does NOT implement yet):
//
//   byte 0-1  "HM"            (magic)
//   byte 2-3  command         (2 bytes, e.g. 0xA3 0x03 for a realtime-data
//                               request; see CMD_* below)
//   byte 4-5  sequence        (uint16 BE, incremented per request)
//   byte 6-7  crc16           (uint16 BE, CRC-16/MODBUS of the payload only)
//   byte 8-9  total_length    (uint16 BE, = payload length + 10)
//   byte 10.. payload         (protobuf-encoded message)
//
// Ported from suaveolent/hoymiles-wifi's DTU.generate_message()/
// parse_response() (Python reference client, verified against the actual
// implementation, not just the .proto message shapes).
// ---------------------------------------------------------------------------
static const uint8_t CMD_HB[2] = {0xA3, 0x02};           // heartbeat request
static const uint8_t CMD_REAL_DATA[2] = {0xA3, 0x03};    // classic realtime data request
static const uint8_t CMD_W_INFO[2] = {0xA3, 0x04};       // alarm-list step 2: fetch the actual warning list
static const uint8_t CMD_COMMAND[2] = {0xA3, 0x05};      // generic command request (power limit, alarm-list step 1, etc.)
static const uint8_t CMD_REAL_DATA_NEW[2] = {0xA3, 0x11};  // paginated realtime data request (richer/diagnostic fields)
// Distinct wire command for a DTU reboot -- ported from ohAnd/dtuGateway,
// which uses this command (NOT CMD_COMMAND above) for
// CMD_ACTION_DTU_REBOOT. Reboots the DTU/inverter's own network stack, not
// the ESP32 this component runs on. See README.md.
static const uint8_t CMD_DTU_REBOOT[2] = {0x23, 0x05};
static const uint16_t DTU_DEFAULT_PORT = 10081;
static const uint8_t FRAME_HEADER_SIZE = 10;
static const size_t MAX_FRAME_SIZE = 512;  // generous -- real RealDataReqDTO/RealDataNewReqDTO frames are well under this
// Upper bound on RealDataNew's paginated response count (`ap` field), purely
// a safety clamp against a malformed/hostile reply -- a single, non-gateway
// HMS-XXXXW is expected to report ap=1 (see README.md).
static const uint8_t REAL_DATA_NEW_MAX_PAGES = 8;

// CommandPB action code for a relative power-limit change -- ported from
// suaveolent/hoymiles-wifi's CMD_ACTION_LIMIT_POWER (hoymiles_wifi/const.py).
// Only the percentage form is implemented (matches async_set_power_limit()):
// the DTU firmware expects "A:<permille>,B:0,C:0\r" in CommandResDTO.data,
// e.g. "A:1000,B:0,C:0\r" for 100.0%.
static const int32_t CMD_ACTION_LIMIT_POWER = 8;

// CommandPB action codes for the alarm-list feature (step 1, sent on
// CMD_COMMAND) and DTU reboot (sent on CMD_DTU_REBOOT) -- both ported from
// ohAnd/dtuGateway's dtuConst.h (CMD_ACTION_ALARM_LIST/CMD_ACTION_DTU_REBOOT).
static const int32_t CMD_ACTION_ALARM_LIST = 50;
static const int32_t CMD_ACTION_DTU_REBOOT = 1;

// Fixed timezone-offset constant (8h in seconds) the DTU firmware expects on
// every OFFSET-bearing request -- same constant used for RealDataNew, not
// something derived from this device's actual timezone.
static const int32_t HMSW_TIME_OFFSET = 28800;

enum class RequestKind : uint8_t {
  NONE = 0,
  HEARTBEAT,
  REAL_DATA,
  POWER_LIMIT,
  REAL_DATA_NEW,
  ALARM_LIST_REQUEST,  // step 1: "please prepare a warning list" (CMD_COMMAND)
  ALARM_LIST_FETCH,    // step 2: "send me that warning list" (CMD_W_INFO)
  DTU_REBOOT,
};

enum class ConnState : uint8_t {
  IDLE = 0,       // nothing in flight, waiting for the next poll
  CONNECTING,     // non-blocking connect() issued, waiting for it to complete
  SENDING,        // connected, request bytes queued, waiting for send() to flush
  RECEIVING,      // request sent, accumulating response bytes
};

class HMSWComponent : public Component {
 public:
  void set_ip_address(const std::string &ip_address) { this->ip_address_ = ip_address; }
  void set_ip_port(uint16_t ip_port) { this->ip_port_ = ip_port; }
  void set_poll_interval(uint32_t ms) { this->poll_interval_ms_ = ms; }
  void set_heartbeat_interval(uint32_t ms) { this->heartbeat_interval_ms_ = ms; }
  void set_request_timeout(uint32_t ms) { this->request_timeout_ms_ = ms; }
  /// Selects which command the periodic poll uses: classic `RealData`
  /// (0xA3 0x03, the default) or the paginated `RealDataNew` (0xA3 0x11),
  /// which additionally exposes energy_today, a power-limit readback, and
  /// diagnostic fields (firmware_version/warning_number/link_status). Kept
  /// as an either/or choice, not both at once, to respect the ~2s minimum
  /// spacing the DTU firmware appears to enforce between any two requests
  /// -- see README.md. Both code paths stay in the component regardless.
  void set_use_real_data_new(bool use_real_data_new) { this->use_real_data_new_ = use_real_data_new; }

  /// Relative power limit, 0-100%. Non-blocking: queues the request, sent
  /// as soon as the current cycle is idle (priority over the next
  /// realtime-data poll -- see loop()), same convention as hm:/hms:.
  /// Named "persistent" because, unlike hm:/hms: (which offer a genuine
  /// RAM-only limit alongside a separate persistent/EEPROM one), no
  /// non-persistent variant of this command is known for HMS-XXXXW --
  /// see README.md. Every call writes to the inverter's EEPROM.
  void set_persistent_power_limit_percent(float percent);

  /// Optional periodic poll of the alarm/warning list (CMD_ACTION_ALARM_LIST,
  /// a two-step request separate from poll_interval/heartbeat_interval --
  /// see README.md). 0 (the default) disables it entirely; the component
  /// still never fetches it unless this is set.
  void set_alarm_poll_interval(uint32_t ms) { this->alarm_poll_interval_ms_ = ms; }

  /// Reboots the DTU/inverter's own network stack -- NOT the ESP32 this
  /// component runs on. Non-blocking, fire-and-forget, same convention as
  /// set_persistent_power_limit_percent(); ported from
  /// ohAnd/dtuGateway's requestRestartDevice(). See README.md.
  void reboot_dtu() { this->dtu_reboot_pending_ = true; }

  /// "Hung DTU" watchdog: a chosen AC-side quantity (see
  /// set_stale_data_use_frequency()) is checked on every realtime-data poll
  /// (both data sources) and a running count of consecutive polls where it
  /// didn't change at all is kept -- ported from a detection method
  /// ohAnd/dtuGateway describes in its own troubleshooting notes ("hanging
  /// detection ... over grid voltage, should be changing at least within 10
  /// consecutive incoming data"). The count is always tracked/published
  /// (see current_stale_data sensor) regardless of this setting, so the
  /// threshold can be tuned from observed behaviour before enabling the
  /// action. 0 (the default) means "track and publish only, never
  /// auto-reboot"; a positive value triggers an automatic reboot_dtu() once
  /// the count reaches it. See README.md.
  void set_stale_data_threshold(uint32_t threshold) { this->stale_data_threshold_ = threshold; }

  /// Which quantity the watchdog above compares across polls. false
  /// (default) = AC/grid voltage, matching ohAnd/dtuGateway's own method --
  /// but that field can be nearly rock-solid on an AC-coupled installation
  /// behind a hybrid inverter, which tightly regulates its own AC output
  /// voltage, making the watchdog prone to false positives there. true =
  /// AC/grid frequency instead: unlike power, it's present and measurable
  /// around the clock (not just while producing, so no night-time
  /// false-positive risk the way power would have), and on a grid-tied
  /// installation the hybrid inverter is normally following/tracking the
  /// actual grid frequency rather than synthesizing its own -- so it still
  /// carries the same natural jitter voltage might not, even when voltage
  /// itself is tightly regulated. See README.md for the trade-off.
  void set_stale_data_use_frequency(bool use_frequency) { this->stale_data_use_frequency_ = use_frequency; }

#ifdef USE_SENSOR
  void set_dc_power_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_power_[ch] = s; }
  void set_dc_current_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_current_[ch] = s; }
  void set_dc_voltage_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_voltage_[ch] = s; }
  void set_dc_energy_total_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_energy_total_[ch] = s; }
  void set_dc_temperature_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_temperature_[ch] = s; }

  void set_ac_voltage_sensor(sensor::Sensor *s) { this->ac_voltage_ = s; }
  void set_ac_current_sensor(sensor::Sensor *s) { this->ac_current_ = s; }
  void set_ac_power_sensor(sensor::Sensor *s) { this->ac_power_ = s; }
  void set_ac_frequency_sensor(sensor::Sensor *s) { this->ac_frequency_ = s; }
  void set_ac_reactive_power_sensor(sensor::Sensor *s) { this->ac_reactive_power_ = s; }
  void set_ac_power_factor_sensor(sensor::Sensor *s) { this->ac_power_factor_ = s; }
  void set_rssi_sensor(sensor::Sensor *s) { this->rssi_ = s; }

  // RealDataNew-only fields. dc_energy_today_/power_limit_readback_/
  // warning_number_/link_status_ stay unpublished (nullptr, never called)
  // when `data_source: real_data` (the default) is in use.
  void set_dc_energy_today_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_energy_today_[ch] = s; }
  void set_power_limit_readback_sensor(sensor::Sensor *s) { this->power_limit_readback_ = s; }
  void set_warning_number_sensor(sensor::Sensor *s) { this->warning_number_ = s; }
  void set_link_status_sensor(sensor::Sensor *s) { this->link_status_ = s; }
  // Alarm-list feature (CMD_ACTION_ALARM_LIST) -- count of currently-active
  // warnings (WTime1 != 0 && WTime2 == 0). Only populated if
  // alarm_poll_interval is set. See README.md.
  void set_active_warning_count_sensor(sensor::Sensor *s) { this->active_warning_count_ = s; }
  // "Hung DTU" watchdog -- current count of consecutive realtime-data polls
  // where AC voltage hasn't changed at all. Always populated (independent
  // of stale_data_reboot_threshold, see set_stale_data_threshold()). See
  // README.md.
  void set_current_stale_data_sensor(sensor::Sensor *s) { this->current_stale_data_ = s; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_reachable_sensor(binary_sensor::BinarySensor *s) { this->reachable_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_firmware_version_sensor(text_sensor::TextSensor *s) { this->firmware_version_ = s; }
  // Semicolon-joined "<label> (code N)" list of currently-active warnings,
  // or "None". See README.md.
  void set_active_warnings_sensor(text_sensor::TextSensor *s) { this->active_warnings_ = s; }
#endif

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  // --- Non-blocking request/response cycle -----------------------------
  // Mirrors the Python reference client: one short-lived TCP connection
  // per request (connect -> send -> read one response -> close), NOT a
  // long-lived socket with a persistent read loop. Simpler and more
  // robust on an ESP32 than trying to keep a multi-minute idle TCP
  // connection alive across WiFi hiccups.
  void start_request_(RequestKind kind);
  void abort_request_(const char *reason);
  void handle_connecting_();
  void handle_sending_();
  void handle_receiving_();
  void on_frame_received_(const uint8_t *cmd, const uint8_t *payload, size_t len);
  void handle_real_data_(const RealDataReqDTO &data);
  void handle_real_data_new_(const RealDataNewReqDTO &data);
  void handle_command_response_(const CommandReqDTO &data);
  void handle_alarm_list_(const WInfoReqDTO &data);
  void check_stale_data_(int32_t raw_ac_voltage, int32_t raw_ac_frequency);
  void publish_reachable_(bool reachable);

  static uint16_t crc16_modbus_(const uint8_t *data, size_t len);
  size_t build_frame_(const uint8_t *cmd, const uint8_t *payload, size_t payload_len, uint8_t *out);

  std::string ip_address_;
  uint16_t ip_port_{DTU_DEFAULT_PORT};
  uint32_t poll_interval_ms_{30000};
  uint32_t heartbeat_interval_ms_{20000};
  uint32_t request_timeout_ms_{3000};
  bool use_real_data_new_{false};
  uint32_t alarm_poll_interval_ms_{0};  // 0 = disabled (default)
  uint32_t last_alarm_poll_{0};

  std::unique_ptr<socket::Socket> socket_{nullptr};
  ConnState state_{ConnState::IDLE};
  RequestKind pending_kind_{RequestKind::NONE};
  uint16_t sequence_{0};

  uint8_t tx_buf_[MAX_FRAME_SIZE]{};
  size_t tx_len_{0};
  size_t tx_sent_{0};

  uint8_t rx_buf_[MAX_FRAME_SIZE]{};
  size_t rx_len_{0};
  uint16_t rx_expected_len_{0};

  uint32_t state_deadline_{0};
  uint32_t last_poll_{0};
  uint32_t last_heartbeat_{0};

  uint32_t consecutive_failures_{0};
  static const uint32_t REACHABLE_FAILURE_THRESHOLD = 3;

  bool power_limit_pending_{false};
  float power_limit_value_{100.0f};

  // RealDataNew pagination (see README.md): the Python reference client
  // reads `ap` (total pages) from the FIRST page's response and then
  // fetches cp=1..ap-1, each over its own short-lived TCP connection, same
  // as every other request in this component. A single, non-gateway
  // HMS-XXXXW is expected to report ap=1 (no extra round trips), but this
  // is implemented for correctness in case it isn't.
  uint8_t real_data_new_cp_{0};
  uint8_t real_data_new_ap_{1};
  bool real_data_new_pending_more_{false};

  // Alarm-list two-step sequence (see README.md): alarm_list_pending_
  // triggers step 1 (request), alarm_list_fetch_pending_ triggers step 2
  // (fetch) once step 1 is acknowledged.
  bool alarm_list_pending_{false};
  bool alarm_list_fetch_pending_{false};

  // DTU reboot -- fire-and-forget, same convention as power_limit_pending_.
  bool dtu_reboot_pending_{false};

  // "Hung DTU" watchdog (see set_stale_data_threshold()/README.md).
  uint32_t stale_data_threshold_{0};  // 0 = track/publish only, never auto-reboot
  bool stale_data_use_frequency_{false};  // false = AC voltage (default), true = AC frequency
  uint32_t stale_data_count_{0};
  int32_t last_stale_value_raw_{0};
  bool has_last_stale_value_{false};

#ifdef USE_SENSOR
  sensor::Sensor *dc_power_[4]{};
  sensor::Sensor *dc_current_[4]{};
  sensor::Sensor *dc_voltage_[4]{};
  sensor::Sensor *dc_energy_total_[4]{};
  sensor::Sensor *dc_temperature_[4]{};
  sensor::Sensor *dc_energy_today_[4]{};  // RealDataNew only

  sensor::Sensor *ac_voltage_{nullptr};
  sensor::Sensor *ac_current_{nullptr};
  sensor::Sensor *ac_power_{nullptr};
  sensor::Sensor *ac_frequency_{nullptr};
  sensor::Sensor *ac_reactive_power_{nullptr};
  sensor::Sensor *ac_power_factor_{nullptr};
  sensor::Sensor *rssi_{nullptr};

  // RealDataNew only -- diagnostic entities, see README.md's entity table.
  sensor::Sensor *power_limit_readback_{nullptr};
  sensor::Sensor *warning_number_{nullptr};
  sensor::Sensor *link_status_{nullptr};
  sensor::Sensor *active_warning_count_{nullptr};  // alarm-list feature only
  sensor::Sensor *current_stale_data_{nullptr};    // "hung DTU" watchdog
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *reachable_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *firmware_version_{nullptr};  // RealDataNew only
  text_sensor::TextSensor *active_warnings_{nullptr};   // alarm-list feature only
#endif
};

}  // namespace hmsw
}  // namespace esphome
