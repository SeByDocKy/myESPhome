#pragma once

#include <functional>
#include <string>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

#include "s21_protocol.h"

namespace esphome {
namespace faikin {

/// Boolean features that are written through the D6 / D7 frames.
enum class Flag : uint8_t { ECONO, POWERFUL, COMFORT, QUIET, STREAMER, SENSOR };

/// Native ESPHome client for the Daikin S21 serial port (phase 1: S21 only).
///
/// The unit is the master of nothing: we poll it, one request at a time, from loop() with a small
/// non-blocking state machine (send, wait for ACK, wait for the response frame, ACK it).
class FaikinComponent : public Component, public uart::UARTDevice {
#ifdef USE_SENSOR
  SUB_SENSOR(home_temperature)
  SUB_SENSOR(outside_temperature)
  SUB_SENSOR(liquid_temperature)
  SUB_SENSOR(fan_speed)
  SUB_SENSOR(compressor)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(connected)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(protocol_version)
  SUB_TEXT_SENSOR(model_code)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(econo)
  SUB_SWITCH(powerful)
  SUB_SWITCH(comfort)
  SUB_SWITCH(quiet)
  SUB_SWITCH(streamer)
  SUB_SWITCH(sensor)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(demand)
#endif

 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_update_interval(uint32_t ms) { this->update_interval_ms_ = ms; }
  void set_response_timeout_ms(uint32_t ms) { this->response_timeout_ms_ = ms; }
  void set_command_gap_ms(uint32_t ms) { this->command_gap_ms_ = ms; }
  void set_probe(bool probe) { this->probe_enabled_ = probe; }

  /// True once the unit answered a status request, false after repeated failures.
  bool is_online() const { return this->online_; }
  /// Last known unit state, including optimistic values after an accepted write.
  const s21::State &get_state() const { return this->state_; }

  /// Control entry points used by the platforms. They return false (and log) when the request cannot
  /// be honoured, for instance when the unit is not connected.
  bool control_power(bool on);
  bool control_mode(s21::Mode mode);  // also powers the unit on
  bool control_target_temperature(float celsius);
  bool control_fan(s21::Fan fan);
  bool control_swing(bool horizontal, bool vertical);
  bool control_flag(Flag flag, bool value);
  bool control_demand(int percent);

  /// Sends an arbitrary two character command, for protocol exploration. The response is logged.
  void send_raw(const std::string &command, const std::string &payload);
  /// Queries the identification and capability commands again and logs the raw answers.
  void run_probe();

  /// Called whenever the unit state changes (after decoding or after an accepted write).
  template<typename F> void add_on_state_callback(F &&callback) {
    this->state_callback_.add(std::forward<F>(callback));
  }

  static constexpr size_t POLL_COUNT = 10;
  static constexpr size_t PROBE_COUNT = 7;

 protected:
  enum class Phase : uint8_t { IDLE, WAIT_ACK, WAIT_STX, RECEIVING };
  enum class TxnKind : uint8_t { POLL, WRITE, PROBE, RAW };

  struct Txn {
    uint8_t c1{0};
    uint8_t c2{0};
    uint8_t payload[s21::MAX_PAYLOAD]{};
    uint8_t plen{0};
    TxnKind kind{TxnKind::POLL};
    uint8_t index{0};         // poll or probe table index
    uint8_t group{0};         // written group for WRITE
    bool cycle{false};        // POLL belongs to the regular cycle (advances the cycle index)
    bool expect_response{true};
  };

  bool start_next_(uint32_t now);
  void begin_txn_(uint32_t now);
  void on_byte_(uint8_t b, uint32_t now);
  void handle_frame_(uint32_t now);
  void finish_ok_(uint32_t now);
  void finish_nak_(uint32_t now);
  void finish_error_(const char *why, uint32_t now);
  void set_online_(bool online, uint32_t now);
  void handle_poll_response_();
  void handle_probe_response_();
  void finish_probe_();
  void publish_entities_();
  void notify_if_changed_(const s21::State &before);
  bool poll_enabled_(size_t index) const;
  bool require_(uint8_t group, const char *what);
  void mark_dirty_(uint8_t group) { this->dirty_ |= group; }
  void discard_rx_();

  // Configuration.
  uint32_t update_interval_ms_{1000};
  uint32_t response_timeout_ms_{500};
  uint32_t command_gap_ms_{25};
  bool probe_enabled_{true};

  // Unit state. `desired_` follows `state_` for every group without a pending write.
  s21::State state_;
  s21::State desired_;
  uint8_t dirty_{0};
  uint8_t refresh_mask_{0};  // poll table indices to re-read right after a write

  // Connection.
  bool online_{false};
  uint8_t errors_{0};

  // Transaction engine.
  Phase phase_{Phase::IDLE};
  Txn txn_;
  s21::FrameParser parser_;
  uint32_t deadline_{0};
  uint32_t next_start_{0};

  // Regular polling cycle.
  size_t poll_index_{POLL_COUNT};
  uint32_t cycle_start_{0};
  uint8_t nak_budget_[POLL_COUNT]{};
  bool poll_ok_[POLL_COUNT]{};

  // Probe.
  bool probe_pending_{false};
  bool probe_done_{false};
  size_t probe_index_{0};
  bool probe_have_version_{false};
  std::string probe_version_;
  std::string probe_f8_;
  std::string probe_model_;

  // Raw command.
  bool raw_pending_{false};
  std::string raw_cmd_;
  std::string raw_payload_;

  CallbackManager<void()> state_callback_;
};

template<typename... Ts> class SendRawAction : public Action<Ts...>, public Parented<FaikinComponent> {
 public:
  explicit SendRawAction(FaikinComponent *parent) : Parented(parent) {}
  TEMPLATABLE_VALUE(std::string, command)
  TEMPLATABLE_VALUE(std::string, payload)

  void play(const Ts &...x) override {
    this->parent_->send_raw(this->command_.value(x...), this->payload_.value(x...));
  }
};

template<typename... Ts> class ProbeAction : public Action<Ts...>, public Parented<FaikinComponent> {
 public:
  explicit ProbeAction(FaikinComponent *parent) : Parented(parent) {}
  void play(const Ts &...x) override { this->parent_->run_probe(); }
};

}  // namespace faikin
}  // namespace esphome
