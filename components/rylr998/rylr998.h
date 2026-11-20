#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/defines.h"
#include "esphome/components/uart/uart.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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

namespace esphome::rylr998 {

// RYLR998 and RYLR999 share the same AT command set on the LoRa side (AT+ADDRESS, AT+BAND,
// AT+PARAMETER, AT+NETWORKID, AT+CRFOP, AT+SEND, +RCV). The only functional difference is the
// maximum RF power: 22 dBm (RYLR998) vs 30 dBm (RYLR999). `variant_` clamps AT+CRFOP so a value
// set at runtime (number entity, automation) can never exceed what the module's PA supports.
enum class RYLRVariant : uint8_t {
  RYLR998 = 0,
  RYLR999 = 1,
};

// ---------------------------------------------------------------------------------------------
// Protocol limits (REYAX LoRa AT command guide)
// ---------------------------------------------------------------------------------------------
// <Data> of AT+SEND is limited to 240 ASCII characters. The component hex-encodes binary data
// (1 byte -> 2 characters) and the module transmits those characters as-is, so the largest binary
// payload is 120 bytes. (A LoRa frame cannot exceed 255 bytes anyway: 240 + 8 added by the module.)
static constexpr size_t MAX_PAYLOAD_CHARS = 240;
static constexpr size_t MAX_PAYLOAD_BYTES = MAX_PAYLOAD_CHARS / 2;
// Bytes the module adds on air to the transmitted <Data> (documented in the AT guide).
static constexpr size_t MODULE_OVERHEAD_BYTES = 8;

// Longest valid "+RCV=<addr>,<len>,<data>,<rssi>,<snr>" line is about 270 characters
// (with the spaces shown in the datasheet example). Anything longer is garbage.
static constexpr size_t RX_LINE_MAX = 300;
// "AT+SEND=65535,240," + 240 data characters + "\r\n" + NUL = 8+5+1+3+1+240+2+1 = 261
static constexpr size_t CMD_BUF_SIZE = 272;

// Pending packets waiting for the radio (fixed storage, no heap allocation per packet).
static constexpr uint8_t TX_QUEUE_SIZE = 4;

class RYLR998Component : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

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
  void set_variant(RYLRVariant variant) { this->variant_ = variant; }
  RYLRVariant get_variant() const { return this->variant_; }

#ifdef USE_SENSOR
  void set_rssi_sensor(sensor::Sensor *s) { this->rssi_sensor_ = s; }
  void set_snr_sensor(sensor::Sensor *s) { this->snr_sensor_ = s; }
  void set_last_error_sensor(sensor::Sensor *s) { this->last_error_sensor_ = s; }
#endif

  // Non-blocking: stores the (clamped) value and lets loop() send AT+CRFOP when the module is idle.
  void apply_tx_power(uint8_t power);
  uint8_t get_tx_power() const { return this->tx_power_; }
#ifdef USE_NUMBER
  void set_tx_power_number(number::Number *n) { this->tx_power_number_ = n; }
#endif

  /// True once the module answered and was configured (false again while it is being re-initialised).
  bool is_ready() const { return this->state_ == State::READY; }

  // Queue a packet for transmission. Returns false when the packet is refused (module not ready,
  // empty or larger than MAX_PAYLOAD_BYTES, queue full). Returns true when it has been queued.
  bool transmit_packet(const std::vector<uint8_t> &data);
  bool transmit_packet(uint16_t destination, const std::vector<uint8_t> &data);

  bool send_data(uint16_t destination, const std::vector<uint8_t> &data);
  bool send_data(uint16_t destination, const std::string &data);

  // Plain C function pointer callback (no vtable, no heap).
  typedef void (*packet_raw_cb_t)(const std::vector<uint8_t> &data, float rssi, float snr);
  void set_raw_packet_callback(packet_raw_cb_t cb) { this->raw_cb_ = cb; }

  // Every on_packet automation declared in YAML is registered here.
  void add_packet_trigger(Trigger<std::vector<uint8_t>, float, float> *trigger) {
    this->packet_triggers_.push_back(trigger);
  }
  // Kept for backward compatibility: now adds a trigger instead of replacing the previous one.
  void set_packet_trigger(Trigger<std::vector<uint8_t>, float, float> *trigger) { this->add_packet_trigger(trigger); }

  template<typename F> void add_on_packet_callback(F &&cb) { this->packet_callback_.add(std::forward<F>(cb)); }

  // Exposed for unit tests.
  uint32_t compute_air_time_ms(size_t chars_sent) const;

 protected:
  enum class State : uint8_t {
    BOOT_WAIT,  // waiting for the module to boot after power-up
    CONFIG,     // sending the configuration sequence
    READY,      // configured, TX queue is served
  };
  enum class CmdKind : uint8_t {
    NONE,
    PING,
    CONFIG,
    TXPOWER,
    SEND,
  };
  struct TxPacket {
    uint16_t destination;
    uint8_t length;
    uint8_t retries;
    uint8_t data[MAX_PAYLOAD_BYTES];
  };

  // --- RX -------------------------------------------------------------------------------------
  void read_uart_(uint32_t now);
  void process_line_(uint32_t now);
  void handle_rcv_(const char *p, size_t n);
  void publish_packet_(uint16_t address, int rssi, int snr);

  // --- command engine (one command in flight, answers handled asynchronously in loop()) -------
  void service_(uint32_t now);
  void dispatch_next_(uint32_t now);
  void send_config_step_(uint32_t now);
  void send_tx_power_(uint32_t now);
  void send_next_packet_(uint32_t now);
  void send_ping_(uint32_t now);
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

  uint8_t max_tx_power_() const { return this->variant_ == RYLRVariant::RYLR999 ? 30 : 22; }
  const char *variant_name_() const { return this->variant_ == RYLRVariant::RYLR999 ? "RYLR999" : "RYLR998"; }
  uint8_t bandwidth_to_code_(uint32_t bandwidth);
  const char *bandwidth_to_string_(uint32_t bandwidth);

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
  RYLRVariant variant_{RYLRVariant::RYLR998};

  // --- engine state ---------------------------------------------------------------------------
  State state_{State::BOOT_WAIT};
  CmdKind cmd_kind_{CmdKind::NONE};
  bool initialized_{false};
  bool tx_power_dirty_{false};
  uint8_t cfg_step_{0};
  uint8_t cfg_attempts_{0};
  uint8_t consecutive_failures_{0};
  uint8_t tx_power_retries_{0};
  bool first_ready_{true};
  bool tx_busy_{false};    // radio busy transmitting (air time of the last AT+SEND not elapsed yet)
  bool gap_active_{false};  // inter-command pause running

  uint32_t boot_deadline_{0};
  uint32_t cmd_deadline_{0};
  uint32_t next_cmd_not_before_{0};
  uint32_t tx_busy_until_{0};
  uint32_t next_ping_at_{0};

  char cmd_buf_[CMD_BUF_SIZE];

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
  packet_raw_cb_t raw_cb_{nullptr};
  std::vector<Trigger<std::vector<uint8_t>, float, float> *> packet_triggers_;
  CallbackManager<void(uint16_t, const std::vector<uint8_t> &, int, int)> packet_callback_;

#ifdef USE_SENSOR
  sensor::Sensor *rssi_sensor_{nullptr};
  sensor::Sensor *snr_sensor_{nullptr};
  sensor::Sensor *last_error_sensor_{nullptr};
#endif
#ifdef USE_NUMBER
  number::Number *tx_power_number_{nullptr};
#endif
};

}  // namespace esphome::rylr998
