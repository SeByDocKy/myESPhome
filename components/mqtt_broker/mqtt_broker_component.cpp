#include "mqtt_broker_component.h"

#include <algorithm>
#include <cerrno>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::mqtt_broker {

static const char *const TAG = "mqtt_broker";

static constexpr size_t READ_CHUNK = 1024;
static constexpr int MAX_READS_PER_LOOP = 8;

uint32_t MqttBrokerComponent::uptime_s() const { return (millis() - this->start_ms_) / 1000; }

void MqttBrokerComponent::setup() {
  BrokerOptions o;
  o.username = this->username_;
  o.password = this->password_;
  o.max_packet_size = this->max_packet_size_;
  this->broker_.set_options(o);
  this->start_ms_ = millis();
  this->start_();
}

void MqttBrokerComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "MQTT broker:\n"
                "  Port: %u\n"
                "  Max clients: %u\n"
                "  Max packet size: %u B\n"
                "  Authentication: %s\n"
                "  Log traffic: %s",
                this->port_, this->max_clients_, this->max_packet_size_, this->username_.empty() ? "off" : "on",
                YESNO(this->log_traffic_));
}

bool MqttBrokerComponent::start_() {
  if (this->listen_socket_ != nullptr)
    return true;

  auto sock = socket::socket_ip_loop_monitored(SOCK_STREAM, 0);
  if (sock == nullptr) {
    if (!this->failed_logged_)
      ESP_LOGW(TAG, "Cannot create the listening socket (no free socket? see CONFIG_LWIP_MAX_SOCKETS)");
    this->failed_logged_ = true;
    return false;
  }
  int enable = 1;
  if (sock->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0)
    ESP_LOGW(TAG, "SO_REUSEADDR failed (errno %d)", errno);
  if (sock->setblocking(false) != 0) {
    ESP_LOGW(TAG, "Cannot set the listening socket non-blocking (errno %d)", errno);
    return false;
  }

  struct sockaddr_storage addr;
  socklen_t len = socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr), this->port_);
  if (len == 0) {
    ESP_LOGW(TAG, "Cannot build the listening address");
    return false;
  }
  if (sock->bind(reinterpret_cast<struct sockaddr *>(&addr), len) != 0) {
    if (!this->failed_logged_)
      ESP_LOGW(TAG, "Cannot bind port %u (errno %d)", this->port_, errno);
    this->failed_logged_ = true;
    return false;
  }
  if (sock->listen(2) != 0) {
    if (!this->failed_logged_)
      ESP_LOGW(TAG, "Cannot listen on port %u (errno %d)", this->port_, errno);
    this->failed_logged_ = true;
    return false;
  }
  this->listen_socket_ = std::move(sock);
  this->failed_logged_ = false;
  ESP_LOGI(TAG, "MQTT broker listening on port %u (max %u clients)", this->port_,
           static_cast<unsigned>(this->max_clients_));
  return true;
}

void MqttBrokerComponent::accept_() {
  // Drain the accept queue (the listening socket is monitored, so loop() is woken up on a new connection)
  for (int i = 0; i < 4; i++) {
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    auto client = this->listen_socket_->accept_loop_monitored(reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
    if (client == nullptr)
      return;
    char peer[socket::SOCKADDR_STR_LEN];
    client->getpeername_to(peer);
    if (this->conns_.size() >= this->max_clients_) {
      ESP_LOGW(TAG, "Connection from %s refused: %u clients already connected", peer,
               static_cast<unsigned>(this->conns_.size()));
      client->close();
      this->stats_.connections_refused++;
      continue;
    }
    this->stats_.connections_total++;
    client->setblocking(false);
    int one = 1;
    client->setsockopt(IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    Connection c;
    c.sock = std::move(client);
    c.session = this->broker_.create_session(millis(), peer);
    c.maybe_more = true;  // data may already be waiting
    this->conns_.push_back(std::move(c));
    ESP_LOGD(TAG, "TCP connection from %s", peer);
  }
}

void MqttBrokerComponent::read_(Connection &c, uint32_t now) {
  uint8_t buf[READ_CHUNK];
  c.maybe_more = false;
  for (int i = 0; i < MAX_READS_PER_LOOP; i++) {
    ssize_t n = c.sock->read(buf, sizeof(buf));
    if (n > 0) {
      this->stats_.bytes_rx += static_cast<uint64_t>(n);
      if (!this->broker_.feed(*c.session, buf, static_cast<size_t>(n), now)) {
        c.session->close_after_flush = true;
        return;
      }
      if (c.session->dead)
        return;
      if (static_cast<size_t>(n) < sizeof(buf))
        return;  // short read: the socket buffer is drained
      continue;
    }
    if (n == 0) {  // peer closed
      c.session->dead = true;
      return;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return;
    if (errno == EINTR)
      continue;
    ESP_LOGD(TAG, "Read error on %s (errno %d)", c.session->peer.c_str(), errno);
    c.session->dead = true;
    return;
  }
  c.maybe_more = true;  // read budget exhausted, come back without waiting for ready()
}

bool MqttBrokerComponent::write_(Connection &c) {
  Session &s = *c.session;
  while (s.tx_pending() > 0) {
    ssize_t n = c.sock->write(s.tx.data() + s.tx_pos, s.tx_pending());
    if (n > 0) {
      this->stats_.bytes_tx += static_cast<uint64_t>(n);
      s.tx_consumed(static_cast<size_t>(n));
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      return true;  // try again next loop
    if (n < 0 && errno == EINTR)
      continue;
    return false;  // error
  }
  return true;
}

void MqttBrokerComponent::close_(size_t index) {
  Connection &c = this->conns_[index];
  Session *s = c.session;
  if (c.sock != nullptr)
    c.sock->close();
  c.sock.reset();
  c.session = nullptr;
  this->conns_.erase(this->conns_.begin() + static_cast<std::ptrdiff_t>(index));
  if (s != nullptr)
    this->broker_.remove_session(s);  // may publish a will message -> on_publish()
}

void MqttBrokerComponent::notify_clients_() {
  size_t n = this->broker_.connected_count();
  if (n == this->last_connected_)
    return;
  this->last_connected_ = n;
  for (auto *sink : this->sinks_)
    sink->on_clients_changed(n);
}

void MqttBrokerComponent::loop() {
  if (!this->start_())
    return;
  const uint32_t now = millis();

  this->accept_();

  for (auto &c : this->conns_) {
    if (c.session == nullptr || c.session->dead)
      continue;
    if (c.maybe_more || c.sock->ready())
      this->read_(c, now);
  }

  this->broker_.tick(now);

  // Write, then remove the sessions that are finished (iterate backwards: close_ erases)
  for (size_t i = this->conns_.size(); i-- > 0;) {
    Connection &c = this->conns_[i];
    bool ok = this->write_(c);
    Session &s = *c.session;
    if (!ok || s.dead || (s.close_after_flush && s.tx_pending() == 0))
      this->close_(i);
  }
  this->notify_clients_();
}

void MqttBrokerComponent::flush_() {
  for (size_t i = this->conns_.size(); i-- > 0;) {
    if (!this->write_(this->conns_[i]))
      this->close_(i);
  }
}

size_t MqttBrokerComponent::publish(const std::string &topic, const std::string &payload) {
  this->stats_.publishes_tx++;
  size_t n = this->broker_.publish(topic, payload);
  this->flush_();
  return n;
}

void MqttBrokerComponent::on_publish(Session & /*from*/, const std::string &topic, const uint8_t *payload, size_t len,
                               uint8_t /*qos*/, bool /*retain*/) {
  this->stats_.publishes_rx++;
  for (auto *sink : this->sinks_)
    sink->on_message(topic, payload, len);
}

void MqttBrokerComponent::on_connect(Session &s) {
  ESP_LOGI(TAG, "MQTT client connected: id='%s' from %s (MQTT level %u, keepalive %us)", s.client_id.c_str(),
           s.peer.c_str(), s.level, s.keepalive_s);
}

void MqttBrokerComponent::on_disconnect(Session &s) {
  ESP_LOGI(TAG, "MQTT client disconnected: id='%s' from %s", s.client_id.c_str(), s.peer.c_str());
}

void MqttBrokerComponent::on_subscribe(Session &s, const std::string &filter, uint8_t qos) {
  ESP_LOGI(TAG, "MQTT client '%s' subscribed to '%s' (QoS %u)", s.client_id.c_str(), filter.c_str(), qos);
  for (auto *sink : this->sinks_)
    sink->on_subscribed(filter);
}

void MqttBrokerComponent::on_log(LogLevel level, const std::string &message) {
  switch (level) {
    case LogLevel::DEBUG:
      ESP_LOGD(TAG, "%s", message.c_str());
      break;
    case LogLevel::TRAFFIC:
      if (this->log_traffic_) {
        ESP_LOGI(TAG, "%s", message.c_str());
      } else {
        ESP_LOGV(TAG, "%s", message.c_str());
      }
      break;
    case LogLevel::WARN:
      ESP_LOGW(TAG, "%s", message.c_str());
      break;
  }
}

}  // namespace esphome::mqtt_broker
