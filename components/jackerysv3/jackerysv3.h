#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

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
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "esphome/components/mqtt_broker/mqtt_broker_component.h"
#include "jackery_state.h"

namespace esphome::jackerysv3 {

/// Hub of the component: decoding of the Jackery SolarVault 3 protocol on top of an `mqtt_broker`.
///
///   Jackery battery  <--MQTT-->  mqtt_broker (on the ESP)  <--native API-->  Home Assistant
///
/// The battery is an MQTT *client*: it connects to the mqtt_broker (configured in the Jackery app), publishes its
/// reports on `<prefix>/device/<sn>/status|event` and listens to `<prefix>/device/<sn>/action`, where this hub
/// publishes the poll requests and the commands.
class JackerySV3Hub : public PollingComponent, public mqtt_broker::MessageSink {
 public:
  // ---- configuration (generated code) -----------------------------------------------------------------------
  void set_sn(const std::string &sn) { this->sn_ = sn; }
  void set_token(const std::string &token) { this->token_ = token; }
  void set_topic_prefix(const std::string &prefix) { this->prefix_ = prefix; }
  void set_mqtt_broker(mqtt_broker::MqttBrokerComponent *broker) { this->broker_ = broker; }
  void set_offline_timeout(uint32_t ms) { this->offline_timeout_ms_ = ms; }
  void set_plug_sn(uint8_t index, const std::string &sn);

  // ---- entity registration (generated code) -----------------------------------------------------------------
#ifdef USE_SENSOR
  void add_sensor(const char *key, uint8_t index, sensor::Sensor *sensor);
#endif
#ifdef USE_BINARY_SENSOR
  void add_binary_sensor(const char *key, binary_sensor::BinarySensor *sensor);
#endif
#ifdef USE_TEXT_SENSOR
  void add_text_sensor(const char *key, uint8_t index, text_sensor::TextSensor *sensor);
#endif
#ifdef USE_SWITCH
  void add_switch(SwitchKind kind, uint8_t index, switch_::Switch *sw);
#endif
#ifdef USE_NUMBER
  void add_number(NumberKind kind, number::Number *number);
#endif
#ifdef USE_SELECT
  void add_select(SelectKind kind, select::Select *select);
#endif

  // ---- commands (called by the platforms) -------------------------------------------------------------------
  /// `{"type":1,"eventId":3,"body":{"cmd":5,"rc":1,"<field>":<value>}}` on the action topic
  bool send_main_command(const char *field, int value);
  /// type 103: switch a smart plug (only when it is connected locally, commMode 1)
  bool send_plug_command(uint8_t index, bool on);
  bool send_reboot() { return this->send_main_command("reboot", 1); }

  // ---- Component --------------------------------------------------------------------------------------------
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // ---- MessageSink ------------------------------------------------------------------------------------------
  void on_message(const std::string &topic, const uint8_t *payload, size_t len) override;
  void on_subscribed(const std::string &filter) override;
  void on_clients_changed(size_t connected) override;

  bool is_online() const { return this->online_; }
  /// Decoded state (nullptr before setup())
  const JackeryState *state() const { return this->state_.get(); }

 protected:
  template<typename E> struct NumEntry {
    E *entity;
    uint8_t kind;
    uint8_t index;
    float last{NAN};
    bool has{false};
  };

  void publish_states_(bool force);
  void housekeeping_(uint32_t now);
  void send_polls_();
  bool publish_(const std::string &payload);
  std::string build_message_(int type, int event_id, bool always_token, const std::function<void(JsonObject)> &body);
  bool ready_to_send_();

  // configuration
  std::string sn_;
  std::string token_;
  std::string prefix_{"hb"};
  uint32_t offline_timeout_ms_{60000};
  std::string pinned_plugs_[MAX_PLUGS];

  // runtime
  mqtt_broker::MqttBrokerComponent *broker_{nullptr};
  std::unique_ptr<JackeryState> state_;
  std::string action_topic_, status_topic_, event_topic_;
  uint32_t start_ms_{0};
  uint32_t last_rx_ms_{0};
  uint32_t last_housekeeping_ms_{0};
  uint32_t last_hint_ms_{0};
  uint32_t last_token_warn_ms_{0};
  bool ever_received_{false};
  bool online_{false};
  bool dirty_{true};
  bool poll_now_{false};
  bool token_error_{false};
  bool no_subscriber_logged_{false};
  size_t clients_{0};
  uint32_t messages_{0};

  // entities
#ifdef USE_SENSOR
  std::vector<NumEntry<sensor::Sensor>> sensors_;
#endif
#ifdef USE_BINARY_SENSOR
  struct BinaryEntry {
    binary_sensor::BinarySensor *entity;
    uint8_t source;  // 0 = decoded state (kind), 1 = device online, 2 = MQTT client connected
    uint8_t kind;
    int8_t last{-1};
  };
  std::vector<BinaryEntry> binary_sensors_;
#endif
#ifdef USE_TEXT_SENSOR
  struct TextEntry {
    text_sensor::TextSensor *entity;
    uint8_t kind;
    uint8_t index;
    std::string last;
    bool has{false};
  };
  std::vector<TextEntry> text_sensors_;
#endif
#ifdef USE_SWITCH
  std::vector<NumEntry<switch_::Switch>> switches_;
#endif
#ifdef USE_NUMBER
  std::vector<NumEntry<number::Number>> numbers_;
#endif
#ifdef USE_SELECT
  struct SelectEntry {
    select::Select *entity;
    uint8_t kind;
    int8_t last{-1};
  };
  std::vector<SelectEntry> selects_;
#endif
};

}  // namespace esphome::jackerysv3
