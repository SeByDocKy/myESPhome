#pragma once

// Minimal embedded MQTT broker core used by the mqtt_broker component.
//
// This file is deliberately free of any ESPHome / socket dependency: it only handles MQTT packets
// (parsing, routing, encoding) on byte buffers. The transport (listening socket, accept, read, write)
// lives in mqtt_broker_component.cpp, which makes this core testable on a host machine.
//
// Supported: MQTT 3.1 (level 3), 3.1.1 (level 4) and 5.0 (level 5, properties are parsed and ignored).
//   CONNECT (user/password, will message, keep-alive), PUBLISH QoS 0/1/2 (acknowledged, delivered once),
//   SUBSCRIBE / UNSUBSCRIBE with the '+' and '#' wildcards, PINGREQ, DISCONNECT.
// Not supported (on purpose): retained messages, persistent sessions, QoS > 0 delivery to subscribers
// (everything the broker forwards is sent at QoS 0), shared subscriptions, topic aliases.

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace esphome::mqtt_broker {

enum class LogLevel : uint8_t {
  DEBUG,    // internal details
  TRAFFIC,  // every CONNECT / SUBSCRIBE / PUBLISH ... (shown at INFO when `log_traffic` is enabled)
  WARN,     // protocol problems, refused clients
};

struct Subscription {
  std::string filter;
  uint8_t qos{0};
};

/// One TCP connection. The transport owns the socket, the broker owns everything else.
struct Session {
  unsigned id{0};
  std::string peer;                 // "ip:port", for the logs only
  std::vector<uint8_t> rx;          // received bytes not parsed yet
  std::vector<uint8_t> tx;          // bytes waiting to be sent
  size_t tx_pos{0};                 // bytes of `tx` already sent (maintained by the transport)
  size_t skip{0};                   // bytes of an oversized PUBLISH still to discard
  bool connected{false};            // CONNECT accepted
  bool graceful{false};             // DISCONNECT received: the will message is not published
  bool close_after_flush{false};    // close once `tx` is flushed (refused CONNECT, protocol error)
  bool dead{false};                 // the broker wants this session removed
  uint8_t level{4};                 // protocol level: 3, 4 or 5
  uint16_t keepalive_s{0};
  uint32_t last_rx_ms{0};
  std::string client_id;
  bool has_will{false};
  std::string will_topic;
  std::vector<uint8_t> will_payload;
  uint8_t will_qos{0};
  std::vector<Subscription> subs;

  size_t tx_pending() const { return this->tx.size() - this->tx_pos; }
  /// Drop the bytes already sent. Called by the transport after a successful write.
  void tx_consumed(size_t n);
};

/// Events raised by the broker. All callbacks run from within feed() / publish() / remove_session().
class Listener {
 public:
  virtual ~Listener() = default;
  /// A client published a message (also called for the will message of a client that vanished).
  virtual void on_publish(Session & /*from*/, const std::string & /*topic*/, const uint8_t * /*payload*/,
                          size_t /*len*/, uint8_t /*qos*/, bool /*retain*/) {}
  virtual void on_connect(Session & /*s*/) {}
  /// Called once for every session that had been accepted, when it is removed.
  virtual void on_disconnect(Session & /*s*/) {}
  /// A client subscribed to `filter` (the SUBACK is queued right after).
  virtual void on_subscribe(Session & /*s*/, const std::string & /*filter*/, uint8_t /*qos*/) {}
  virtual void on_log(LogLevel /*level*/, const std::string & /*message*/) {}
};

struct BrokerOptions {
  std::string username;           // empty: no authentication
  std::string password;
  size_t max_packet_size{8192};   // larger PUBLISH packets are discarded, other large packets drop the client
  size_t max_tx_backlog{32768};   // a client that does not read its data is dropped past this size
  uint32_t connect_timeout_ms{10000};  // a connection that does not send CONNECT in time is dropped
};

class Broker {
 public:
  explicit Broker(Listener *listener) : listener_(listener) {}

  void set_options(const BrokerOptions &options) { this->options_ = options; }
  const BrokerOptions &options() const { return this->options_; }

  /// Register a new connection.
  Session *create_session(uint32_t now_ms, const std::string &peer);
  /// Remove a session (publishes its will message when it did not disconnect cleanly).
  void remove_session(Session *session);

  /// Feed bytes received from the socket. Returns false when the session must be closed
  /// (queued bytes, e.g. a CONNACK with an error, should still be flushed first).
  bool feed(Session &session, const uint8_t *data, size_t len, uint32_t now_ms);

  /// Publish a message from the broker itself (QoS 0). Returns the number of sessions it was delivered to.
  size_t publish(const std::string &topic, const uint8_t *payload, size_t len);
  size_t publish(const std::string &topic, const std::string &payload) {
    return this->publish(topic, reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
  }

  /// Number of sessions subscribed to a filter matching `topic`.
  size_t subscriber_count(const std::string &topic) const;

  /// Keep-alive and CONNECT time-outs: marks the expired sessions as dead.
  void tick(uint32_t now_ms);

  std::vector<std::unique_ptr<Session>> &sessions() { return this->sessions_; }
  size_t connected_count() const;

  /// MQTT topic filter matching ('+' one level, '#' the rest; '$' topics are not matched by a leading wildcard).
  static bool topic_matches(const std::string &filter, const std::string &topic);
  static bool valid_filter(const std::string &filter);
  static bool valid_topic_name(const std::string &topic);

 protected:
  bool handle_packet_(Session &s, uint8_t type, uint8_t flags, const uint8_t *body, size_t len, uint32_t now_ms);
  bool handle_connect_(Session &s, const uint8_t *body, size_t len);
  bool handle_publish_(Session &s, uint8_t flags, const uint8_t *body, size_t len);
  bool handle_subscribe_(Session &s, const uint8_t *body, size_t len);
  bool handle_unsubscribe_(Session &s, const uint8_t *body, size_t len);

  void connack_(Session &s, uint8_t v4_code, uint8_t v5_code);
  size_t route_(const std::string &topic, const uint8_t *payload, size_t len);
  void queue_(Session &s, const std::vector<uint8_t> &bytes);
  void log_(LogLevel level, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

  Listener *listener_;
  BrokerOptions options_;
  std::vector<std::unique_ptr<Session>> sessions_;
  unsigned next_id_{1};
};

}  // namespace esphome::mqtt_broker
