#pragma once

// ESPHome component: an MQTT broker running on the ESP itself.
//
// This file is the TCP transport (listening socket, accept, read, write) around the broker core of
// mqtt_broker.h. Other components (e.g. jackerysv3, mqtt_broker_stats) point to it with `mqtt_broker_id:` and
// register a MessageSink to receive the messages / events, and call publish() to send messages.

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include <memory>
#include <string>
#include <vector>

#include "esphome/components/socket/socket.h"
#include "mqtt_broker.h"

namespace esphome::mqtt_broker {

/// Receives the messages and events of a broker. All callbacks run from within the broker's loop(): they must
/// not call publish() synchronously (set a flag and publish from the sink's own loop()).
class MessageSink {
 public:
  virtual ~MessageSink() = default;
  /// A client published `topic` (also called for the will message of a client that vanished).
  virtual void on_message(const std::string &topic, const uint8_t *payload, size_t len) = 0;
  /// A client subscribed to `filter`.
  virtual void on_subscribed(const std::string & /*filter*/) {}
  /// The number of connected MQTT clients changed.
  virtual void on_clients_changed(size_t /*connected*/) {}
};

/// Counters since boot (read by mqtt_broker_stats)
struct BrokerStats {
  uint64_t bytes_rx{0};
  uint64_t bytes_tx{0};
  uint32_t publishes_rx{0};      // PUBLISH packets received from clients
  uint32_t publishes_tx{0};      // messages published by the ESP (publish())
  uint32_t connections_total{0};  // TCP connections accepted
  uint32_t connections_refused{0};
};

class MqttBrokerComponent : public Component, public Listener {
 public:
  // ---- configuration (generated code) -----------------------------------------------------------------------
  void set_port(uint16_t port) { this->port_ = port; }
  void set_username(const std::string &username) { this->username_ = username; }
  void set_password(const std::string &password) { this->password_ = password; }
  void set_max_clients(uint8_t n) { this->max_clients_ = n; }
  void set_max_packet_size(uint16_t n) { this->max_packet_size_ = n; }
  void set_log_traffic(bool enable) { this->log_traffic_ = enable; }
  /// Register a consumer of the messages (generated code)
  void add_sink(MessageSink *sink) { this->sinks_.push_back(sink); }

  // ---- API for the components using the broker --------------------------------------------------------------
  /// Publish a message (QoS 0) to the subscribed clients. Returns the number of clients it was delivered to.
  size_t publish(const std::string &topic, const std::string &payload);
  size_t subscriber_count(const std::string &topic) const { return this->broker_.subscriber_count(topic); }
  size_t connected_count() const { return this->broker_.connected_count(); }
  bool is_listening() const { return this->listen_socket_ != nullptr; }
  uint16_t port() const { return this->port_; }
  const BrokerStats &stats() const { return this->stats_; }
  uint32_t uptime_s() const;

  // ---- Component --------------------------------------------------------------------------------------------
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // ---- Listener ---------------------------------------------------------------------------------------
  void on_publish(Session &from, const std::string &topic, const uint8_t *payload, size_t len, uint8_t qos,
                  bool retain) override;
  void on_connect(Session &s) override;
  void on_disconnect(Session &s) override;
  void on_subscribe(Session &s, const std::string &filter, uint8_t qos) override;
  void on_log(LogLevel level, const std::string &message) override;

 protected:
  struct Connection {
    std::unique_ptr<socket::Socket> sock;
    Session *session{nullptr};
    bool maybe_more{false};  // stopped reading before EAGAIN: retry without waiting for ready()
  };

  bool start_();
  void accept_();
  void read_(Connection &c, uint32_t now);
  bool write_(Connection &c);
  void flush_();
  void close_(size_t index);
  void notify_clients_();

  uint16_t port_{1883};
  std::string username_;
  std::string password_;
  uint8_t max_clients_{4};
  uint16_t max_packet_size_{8192};
  bool log_traffic_{false};

  Broker broker_{this};
  std::unique_ptr<socket::ListenSocket> listen_socket_;
  std::vector<Connection> conns_;
  std::vector<MessageSink *> sinks_;
  BrokerStats stats_;
  uint32_t start_ms_{0};
  bool failed_logged_{false};
  size_t last_connected_{0};
};

}  // namespace esphome::mqtt_broker
