#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32
#include <esp_gattc_api.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#ifdef USE_BINARY_SENSOR
namespace esphome::binary_sensor {
class BinarySensor;
}  // namespace esphome::binary_sensor
#endif
#ifdef USE_SENSOR
namespace esphome::sensor {
class Sensor;
}  // namespace esphome::sensor
#endif
#ifdef USE_NUMBER
namespace esphome::number {
class Number;
}  // namespace esphome::number
#endif

namespace esphome::rylr999_ble {

namespace espbt = esphome::esp32_ble_tracker;

// ---------------------------------------------------------------------------------------------
// Protocol limits (REYAX LoRa AT command guide)
// ---------------------------------------------------------------------------------------------
// <Data> of AT+SEND is limited to 240 ASCII characters. Binary payloads are hex-encoded
// (1 byte -> 2 characters), so the largest binary payload is 120 bytes. The BLE MTU does not limit
// it: the command is written in several GATT chunks (the module is a transparent serial bridge).
static constexpr size_t MAX_PAYLOAD_CHARS = 240;
static constexpr size_t MAX_PAYLOAD_BYTES = MAX_PAYLOAD_CHARS / 2;
// Bytes the module adds on air to the transmitted <Data>.
static constexpr size_t MODULE_OVERHEAD_BYTES = 8;
// Longest valid "+RCV=<addr>,<len>,<data>,<rssi>,<snr>" line is about 270 characters.
static constexpr size_t RX_LINE_MAX = 300;
// "AT+SEND=65535,240," + 240 data characters + "\r\n" + NUL
static constexpr size_t CMD_BUF_SIZE = 272;
// Pending packets waiting for the radio (fixed storage, no heap allocation per packet).
static constexpr uint8_t TX_QUEUE_SIZE = 4;
// ATT payload of a write without response is MTU - 3. Until the MTU is known (default 23), 20 bytes.
static constexpr uint16_t DEFAULT_WRITE_CHUNK = 20;

// Observer interface used by the packet_transport platform to receive decoded packets.
class RYLR999BLEListener {
 public:
  virtual ~RYLR999BLEListener() = default;
  virtual void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) = 0;
};

class RYLR999BLEComponent : public Component, public ble_client::BLEClientNode {
 public:
  void loop() override;
  void dump_config() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  // Service / characteristic UUIDs (write characteristic and, optionally, a distinct notify one).
  void set_service_uuid16(uint16_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_service_uuid32(uint32_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_service_uuid128(uint8_t *uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
  void set_char_uuid16(uint16_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_char_uuid32(uint32_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_char_uuid128(uint8_t *uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
  void set_notify_char_uuid16(uint16_t uuid) { this->notify_char_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_notify_char_uuid32(uint32_t uuid) { this->notify_char_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_notify_char_uuid128(uint8_t *uuid) { this->notify_char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  // LoRa configuration (sent over BLE with AT commands).
  void set_address(uint16_t address) { this->address_ = address; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_spreading_factor(uint8_t sf) { this->spreading_factor_ = sf; }
  void set_bandwidth(uint32_t bw) { this->bandwidth_ = bw; }
  void set_coding_rate(uint8_t cr) { this->coding_rate_ = cr; }
  void set_preamble_length(uint8_t pl) { this->preamble_length_ = pl; }
  void set_network_id(uint8_t nid) { this->network_id_ = nid; }
  void set_tx_power(uint8_t tp) { this->tx_power_ = tp; }
  // true: TX guard time = computed air time of each packet. false: fixed 600 ms (legacy).
  void set_air_time(bool enable) { this->compute_air_time_ = enable; }
  // Optional extra floor (ms) on the spacing between two AT+SEND. Packets wait in the queue, they are
  // never dropped because of it.
  void set_min_tx_interval_ms(uint32_t ms) { this->min_tx_interval_ms_ = ms; }

#ifdef USE_BINARY_SENSOR
  void set_lora_ready_binary_sensor(binary_sensor::BinarySensor *sensor) { this->lora_ready_binary_sensor_ = sensor; }
#endif
#ifdef USE_SENSOR
  void set_rssi_sensor(sensor::Sensor *s) { this->rssi_sensor_ = s; }
  void set_snr_sensor(sensor::Sensor *s) { this->snr_sensor_ = s; }
  void set_last_error_sensor(sensor::Sensor *s) { this->last_error_sensor_ = s; }
#endif
#ifdef USE_NUMBER
  void set_tx_power_number(number::Number *n) { this->tx_power_number_ = n; }
#endif

  // Non-blocking: stores the (clamped) value and lets loop() send AT+CRFOP when the module is idle.
  void apply_tx_power(uint8_t power);
  uint8_t get_tx_power() const { return this->tx_power_; }

  /// Asks the module to restart (AT+RESET). Non-blocking: the whole configuration is re-applied when
  /// the module reports +READY (or when the BLE link is re-established).
  void request_reset() { this->reset_requested_ = true; }

  void register_listener(RYLR999BLEListener *listener) { this->listeners_.push_back(listener); }

  bool is_lora_ready() const { return this->state_ == State::READY; }
  size_t get_max_packet_size() const { return MAX_PAYLOAD_BYTES; }

  // Queues a packet (hex-encoded, AT+SEND=0,...: LoRa broadcast, packet_transport identifies the
  // peer itself). Returns false if refused: module not ready, empty, larger than
  // get_max_packet_size(), or queue full.
  bool send_payload(const std::vector<uint8_t> &payload);

  // Exposed for unit tests.
  uint32_t compute_air_time_ms(size_t chars_sent) const;

 protected:
  enum class State : uint8_t {
    LINK_DOWN,  // BLE not connected / services not discovered yet
    CONFIG,     // sending the configuration sequence
    READY,      // configured, TX queue is served
  };
  enum class CmdKind : uint8_t {
    NONE,
    PING,
    CONFIG,
    TXPOWER,
    SEND,
    RESET,
  };
  struct TxPacket {
    uint8_t length;
    uint8_t retries;
    uint8_t data[MAX_PAYLOAD_BYTES];
  };

  // --- link (BLE) -----------------------------------------------------------------------------
  void on_link_up_(uint32_t now);
  void on_link_down_();
  // Writes one GATT chunk. Virtual so that the host unit tests can replace the BLE stack.
  virtual bool write_chunk_(const uint8_t *data, uint16_t len);
  void pump_tx_(uint32_t now);

  // --- RX -------------------------------------------------------------------------------------
  void feed_rx_(const uint8_t *data, size_t len);
  void process_line_(uint32_t now);
  void handle_rcv_(const char *p, size_t n);
  void publish_packet_(int rssi, int snr);

  // --- command engine (one command in flight, answers handled asynchronously in loop()) -------
  void service_(uint32_t now);
  void dispatch_next_(uint32_t now);
  void send_config_step_(uint32_t now);
  void send_tx_power_(uint32_t now);
  void send_next_packet_(uint32_t now);
  void send_ping_(uint32_t now);
  void send_reset_(uint32_t now);
  void start_command_(CmdKind kind, size_t len, uint32_t now, uint32_t timeout_ms);
  void on_ok_(uint32_t now);
  void on_error_(int code, uint32_t now);
  void on_timeout_(uint32_t now);
  void on_module_ready_(uint32_t now);
  void advance_config_(uint32_t now);
  void restart_config_(uint32_t now, uint32_t delay_ms);
  void finish_command_(uint32_t now, uint32_t gap_ms);
  void pop_packet_();
  void note_activity_(uint32_t now);
  void set_ready_(bool ready);

  uint8_t bandwidth_to_code_() const;
  static const char *bandwidth_to_string_(uint32_t bandwidth);

  // --- BLE ------------------------------------------------------------------------------------
  espbt::ESPBTUUID service_uuid_;
  espbt::ESPBTUUID char_uuid_;
  espbt::ESPBTUUID notify_char_uuid_;
  uint16_t write_char_handle_{0};
  uint16_t notify_char_handle_{0};
  esp_gatt_write_type_t write_type_{ESP_GATT_WRITE_TYPE_NO_RSP};
  uint16_t write_chunk_size_{DEFAULT_WRITE_CHUNK};

  // --- configuration --------------------------------------------------------------------------
  uint16_t address_{0};
  uint32_t frequency_{915000000};
  uint8_t spreading_factor_{9};
  uint32_t bandwidth_{125000};
  uint8_t coding_rate_{1};
  uint8_t preamble_length_{12};
  uint8_t network_id_{18};
  uint8_t tx_power_{22};
  bool compute_air_time_{true};
  uint32_t min_tx_interval_ms_{0};

  // --- engine state ---------------------------------------------------------------------------
  State state_{State::LINK_DOWN};
  CmdKind cmd_kind_{CmdKind::NONE};
  bool tx_power_dirty_{false};
  bool reset_requested_{false};
  bool reset_expected_{false};
  bool tx_busy_{false};
  bool gap_active_{false};
  bool ready_published_{false};
  bool ready_state_{false};
  bool error_published_{false};
  uint8_t cfg_step_{0};
  uint8_t cfg_attempts_{0};
  uint8_t consecutive_failures_{0};
  uint8_t tx_power_retries_{0};

  uint32_t cmd_deadline_{0};
  uint32_t cmd_timeout_ms_{0};
  uint32_t write_started_at_{0};
  uint32_t next_cmd_not_before_{0};
  uint32_t tx_busy_until_{0};
  uint32_t next_ping_at_{0};

  // Command being written (possibly over several GATT chunks).
  char cmd_buf_[CMD_BUF_SIZE];
  uint16_t out_len_{0};
  uint16_t out_pos_{0};

  // --- RX line assembly (no heap) -------------------------------------------------------------
  char rx_line_[RX_LINE_MAX + 1];
  uint16_t rx_len_{0};
  bool rx_overflow_{false};
  std::vector<uint8_t> rx_data_;  // reused for every received packet

  // --- TX queue -------------------------------------------------------------------------------
  TxPacket tx_queue_[TX_QUEUE_SIZE];
  uint8_t tx_head_{0};
  uint8_t tx_count_{0};

  // --- outputs --------------------------------------------------------------------------------
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *lora_ready_binary_sensor_{nullptr};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *rssi_sensor_{nullptr};
  sensor::Sensor *snr_sensor_{nullptr};
  sensor::Sensor *last_error_sensor_{nullptr};
#endif
#ifdef USE_NUMBER
  number::Number *tx_power_number_{nullptr};
#endif
  std::vector<RYLR999BLEListener *> listeners_{};
};

}  // namespace esphome::rylr999_ble

#endif
