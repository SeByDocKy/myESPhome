# `mqtt_broker` — MQTT broker running on the ESP32

A small MQTT broker embedded in an ESPHome node. Devices that speak MQTT (for example the Jackery SolarVault 3,
see the [`jackerysv3`](../jackerysv3/README.md) component) connect to it over the LAN, and other ESPHome
components point to it with `mqtt_broker_id:` to decode / publish the messages and expose the result as native
ESPHome entities. Home Assistant sees plain ESPHome entities through the native API: no MQTT integration and no
external broker (Mosquitto) are needed.

```
 MQTT device  <--- MQTT --->  mqtt_broker (ESP32)  <--- component using it --->  ESPHome API / Home Assistant
```

## Supported MQTT features

| Supported | Not supported |
|---|---|
| MQTT 3.1 (level 3), 3.1.1 (level 4), 5.0 (level 5: properties are parsed and ignored) | TLS |
| CONNECT with optional user name / password, keep-alive (1.5x), will message, client takeover | Retained messages |
| PUBLISH QoS 0 / 1 / 2 (acknowledged, delivered once) | Persistent sessions |
| SUBSCRIBE / UNSUBSCRIBE with the `+` and `#` wildcards | Delivery to subscribers at QoS > 0 (everything is forwarded at QoS 0) |
| PINGREQ, DISCONNECT | Shared subscriptions, topic aliases |

The protocol core (`mqtt_broker.cpp`) works on byte buffers only and has no socket or ESPHome dependency; it is
tested on a host machine (`tests/`, with AddressSanitizer / UBSan). The TCP transport
(`mqtt_broker_component.cpp`) uses ESPHome's non-blocking socket API and is driven from the main loop.

## Configuration

```yaml
mqtt_broker:
  id: broker
  port: 1883            # default 1883
  # username: user      # optional; give both username and password to require authentication
  # password: secret    # without credentials any client is accepted
  max_clients: 4        # default 4 (1..8) simultaneous connections, extra ones are refused
  max_packet_size: 8192 # default 8192 bytes: larger PUBLISH packets are discarded
  log_traffic: false    # log every CONNECT / SUBSCRIBE / PUBLISH (payload preview, never the password) at INFO
```

`mqtt_broker` is `MULTI_CONF`: use one block per TCP port. The component is ESP32 only.

Sockets: the listening socket plus `max_clients + 1` sockets are reserved (ESPHome raises
`CONFIG_LWIP_MAX_SOCKETS` automatically). Keep `max_clients` low on a busy node.

## Using the broker from another component

In YAML, the component points to the broker with `mqtt_broker_id: broker`. In C++ it registers a
`mqtt_broker::MessageSink` and publishes through the `MqttBrokerComponent`:

```cpp
#include "esphome/components/mqtt_broker/mqtt_broker_component.h"

class MyHub : public Component, public mqtt_broker::MessageSink {
 public:
  void set_mqtt_broker(mqtt_broker::MqttBrokerComponent *b) { this->broker_ = b; }
  void setup() override { this->broker_->add_sink(this); }

  // Called from the broker's loop(): do not publish from here, set a flag and publish from loop().
  void on_message(const std::string &topic, const uint8_t *payload, size_t len) override { /* ... */ }
  void on_subscribed(const std::string &filter) override {}        // optional
  void on_clients_changed(size_t connected) override {}            // optional

 protected:
  mqtt_broker::MqttBrokerComponent *broker_{nullptr};
};

// later: size_t delivered = this->broker_->publish("some/topic", "payload");
```

`MqttBrokerComponent` also exposes `subscriber_count(topic)`, `connected_count()`, `is_listening()`, `port()` and
`stats()` (bytes / publishes / connections counters, intended for a future `mqtt_broker_stats` component).

## Caveats

- Only QoS 0 is forwarded and nothing is retained: enough for request / report protocols, not a Mosquitto
  replacement.
- No TLS: devices that only offer a TLS connection cannot use it.
- A device connects to one broker at a time: if it is configured for this ESP, it no longer talks to another
  broker.
- Memory: each client costs a socket plus rx / tx buffers (bounded by `max_packet_size` and a 32 kB tx backlog
  limit after which a client that does not read is dropped).
