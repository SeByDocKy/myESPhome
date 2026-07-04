#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <vector>
#include <string>
#include <deque>

namespace esphome {
namespace ryuw122 {

enum RYUW122Mode {
  RYUW122_MODE_TAG = 0,
  RYUW122_MODE_ANCHOR = 1,
  RYUW122_MODE_SLEEP = 2,
};

// Command queue entry: raw AT command (without \r\n), sent one at a time,
// waiting for +OK / +ERR before the next one is sent. This mirrors the
// rate-limiting approach used on the RYLR998 component so we never flood
// the module's small UART RX buffer.
struct RYUW122PendingCommand {
  std::string command;
};

class RYUW122Component : public uart::UARTDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // ---- YAML config setters ----
  void set_mode(int mode) { this->mode_ = static_cast<RYUW122Mode>(mode); }
  void set_network_id(const std::string &id) { this->network_id_ = id; }
  void set_address(const std::string &addr) { this->address_ = addr; }
  void set_password(const std::string &pw) { this->password_ = pw; this->has_password_ = true; }
  void set_channel(int channel) { this->channel_ = channel; }
  void set_bandwidth(int bandwidth) { this->bandwidth_ = bandwidth; }
  void set_tx_power(int power) { this->tx_power_ = power; }
  void set_calibration(int cal) { this->calibration_ = cal; }
  void set_rssi_enable(bool enable) { this->rssi_enable_ = enable; }
  void set_tag_duty_cycle(uint32_t enable_ms, uint32_t disable_ms) {
    this->tag_rf_enable_ms_ = enable_ms;
    this->tag_rf_disable_ms_ = disable_ms;
  }

#ifdef USE_SENSOR
  void set_distance_sensor(sensor::Sensor *s) { this->distance_sensor_ = s; }
  void set_rssi_sensor(sensor::Sensor *s) { this->rssi_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_last_data_text_sensor(text_sensor::TextSensor *s) { this->last_data_text_sensor_ = s; }
#endif

  // ---- Public API for actions / lambdas ----
  void tag_send(const std::string &payload);
  void anchor_send(const std::string &tag_address, const std::string &payload);

  // ---- Triggers ----
  void add_on_anchor_rcv_callback(std::function<void(std::string, std::string, float, int)> cb) {
    this->anchor_rcv_callback_.add(std::move(cb));
  }
  void add_on_tag_rcv_callback(std::function<void(std::string, int)> cb) {
    this->tag_rcv_callback_.add(std::move(cb));
  }

 protected:
  // Configuration state machine run once at boot to push all AT+ settings.
  enum class SetupState {
    BEGIN,
    SET_MODE,
    SET_NETWORKID,
    SET_ADDRESS,
    SET_CPIN,
    SET_CHANNEL,
    SET_BANDWIDTH,
    SET_CRFOP,
    SET_CAL,
    SET_RSSI,
    SET_TAGD,
    DONE,
  } setup_state_{SetupState::BEGIN};

  void advance_setup_();
  void enqueue_command_(const std::string &cmd);
  void send_next_command_();
  void process_line_(const std::string &line);
  void handle_ok_();
  void handle_err_(const std::string &line);
  void handle_anchor_rcv_(const std::string &line);
  void handle_tag_rcv_(const std::string &line);

  // UART RX buffering
  std::string rx_buffer_;
  static const size_t MAX_RX_BUFFER = 256;

  // Outgoing command queue / rate limiting
  std::deque<RYUW122PendingCommand> command_queue_;
  bool waiting_for_response_{false};
  uint32_t last_command_sent_ms_{0};
  uint32_t command_timeout_ms_{1500};
  static const uint32_t MIN_COMMAND_INTERVAL_MS = 120;

  // Config
  RYUW122Mode mode_{RYUW122_MODE_TAG};
  std::string network_id_{"REYAX123"};
  std::string address_{"DAVID123"};
  std::string password_;
  bool has_password_{false};
  int channel_{5};
  int bandwidth_{0};
  int tx_power_{5};
  int calibration_{0};
  bool rssi_enable_{true};
  uint32_t tag_rf_enable_ms_{0};
  uint32_t tag_rf_disable_ms_{0};

#ifdef USE_SENSOR
  sensor::Sensor *distance_sensor_{nullptr};
  sensor::Sensor *rssi_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *last_data_text_sensor_{nullptr};
#endif

  CallbackManager<void(std::string, std::string, float, int)> anchor_rcv_callback_;
  CallbackManager<void(std::string, int)> tag_rcv_callback_;
};

class RYUW122AnchorRcvTrigger : public Trigger<std::string, std::string, float, int> {
 public:
  explicit RYUW122AnchorRcvTrigger(RYUW122Component *parent) {
    parent->add_on_anchor_rcv_callback(
        [this](std::string tag_address, std::string payload, float distance, int rssi) {
          this->trigger(tag_address, payload, distance, rssi);
        });
  }
};

class RYUW122TagRcvTrigger : public Trigger<std::string, int> {
 public:
  explicit RYUW122TagRcvTrigger(RYUW122Component *parent) {
    parent->add_on_tag_rcv_callback(
        [this](std::string payload, int rssi) { this->trigger(payload, rssi); });
  }
};

template<typename... Ts> class RYUW122TagSendAction : public Action<Ts...> {
 public:
  void set_parent(RYUW122Component *parent) { this->parent_ = parent; }
  TEMPLATABLE_VALUE(std::string, payload)

  void play(Ts... x) override { this->parent_->tag_send(this->payload_.value(x...)); }

 protected:
  RYUW122Component *parent_;
};

template<typename... Ts> class RYUW122AnchorSendAction : public Action<Ts...> {
 public:
  void set_parent(RYUW122Component *parent) { this->parent_ = parent; }
  TEMPLATABLE_VALUE(std::string, tag_address)
  TEMPLATABLE_VALUE(std::string, payload)

  void play(Ts... x) override {
    this->parent_->anchor_send(this->tag_address_.value(x...), this->payload_.value(x...));
  }

 protected:
  RYUW122Component *parent_;
};

}  // namespace ryuw122
}  // namespace esphome
