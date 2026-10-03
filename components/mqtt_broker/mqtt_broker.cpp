#include "mqtt_broker.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace esphome::mqtt_broker {

namespace {

// MQTT control packet types (high nibble of the first byte)
enum : uint8_t {
  PKT_CONNECT = 1,
  PKT_CONNACK = 2,
  PKT_PUBLISH = 3,
  PKT_PUBACK = 4,
  PKT_PUBREC = 5,
  PKT_PUBREL = 6,
  PKT_PUBCOMP = 7,
  PKT_SUBSCRIBE = 8,
  PKT_SUBACK = 9,
  PKT_UNSUBSCRIBE = 10,
  PKT_UNSUBACK = 11,
  PKT_PINGREQ = 12,
  PKT_PINGRESP = 13,
  PKT_DISCONNECT = 14,
};

// Bounds-checked reader over one packet body. Any short read clears `ok` and the caller drops the packet.
struct Reader {
  const uint8_t *p;
  size_t n;
  size_t i{0};
  bool ok{true};

  Reader(const uint8_t *data, size_t len) : p(data), n(len) {}

  size_t left() const { return this->n - this->i; }
  const uint8_t *cur() const { return this->p + this->i; }

  uint8_t u8() {
    if (this->left() < 1) {
      this->ok = false;
      return 0;
    }
    return this->p[this->i++];
  }
  uint16_t u16() {
    if (this->left() < 2) {
      this->ok = false;
      this->i = this->n;
      return 0;
    }
    uint16_t v = static_cast<uint16_t>((this->p[this->i] << 8) | this->p[this->i + 1]);
    this->i += 2;
    return v;
  }
  bool skip(size_t k) {
    if (this->left() < k) {
      this->ok = false;
      this->i = this->n;
      return false;
    }
    this->i += k;
    return true;
  }
  /// 2-byte length prefixed string
  bool str(std::string &out) {
    uint16_t l = this->u16();
    if (!this->ok || this->left() < l) {
      this->ok = false;
      return false;
    }
    out.assign(reinterpret_cast<const char *>(this->cur()), l);
    this->i += l;
    return true;
  }
  /// 2-byte length prefixed binary data
  bool bin(std::vector<uint8_t> &out) {
    uint16_t l = this->u16();
    if (!this->ok || this->left() < l) {
      this->ok = false;
      return false;
    }
    out.assign(this->cur(), this->cur() + l);
    this->i += l;
    return true;
  }
  bool varint(uint32_t &out) {
    out = 0;
    uint32_t mul = 1;
    for (int k = 0; k < 4; k++) {
      uint8_t b = this->u8();
      if (!this->ok)
        return false;
      out += (b & 0x7F) * mul;
      if (!(b & 0x80))
        return true;
      mul *= 128;
    }
    this->ok = false;
    return false;
  }
  /// MQTT 5 property block: length + content, ignored
  bool skip_props() {
    uint32_t l;
    if (!this->varint(l))
      return false;
    return this->skip(l);
  }
};

void put_u16(std::vector<uint8_t> &v, uint16_t x) {
  v.push_back(static_cast<uint8_t>(x >> 8));
  v.push_back(static_cast<uint8_t>(x & 0xFF));
}

void put_varint(std::vector<uint8_t> &v, uint32_t x) {
  do {
    uint8_t b = x % 128;
    x /= 128;
    if (x > 0)
      b |= 0x80;
    v.push_back(b);
  } while (x > 0);
}

/// Printable preview of a payload for the traffic log
std::string preview(const uint8_t *data, size_t len, size_t max = 200) {
  std::string out;
  size_t n = std::min(len, max);
  out.reserve(n + 3);
  for (size_t i = 0; i < n; i++)
    out.push_back((data[i] >= 0x20 && data[i] < 0x7F) ? static_cast<char>(data[i]) : '.');
  if (len > max)
    out += "...";
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Session
// ---------------------------------------------------------------------------------------------------------------

void Session::tx_consumed(size_t n) {
  this->tx_pos += n;
  if (this->tx_pos >= this->tx.size()) {
    this->tx.clear();
    this->tx_pos = 0;
  } else if (this->tx_pos >= 2048) {
    this->tx.erase(this->tx.begin(), this->tx.begin() + static_cast<std::ptrdiff_t>(this->tx_pos));
    this->tx_pos = 0;
  }
}

// ---------------------------------------------------------------------------------------------------------------
// Topic helpers
// ---------------------------------------------------------------------------------------------------------------

bool Broker::valid_topic_name(const std::string &topic) {
  if (topic.empty())
    return false;
  for (char c : topic) {
    if (c == '+' || c == '#' || c == '\0')
      return false;
  }
  return true;
}

bool Broker::valid_filter(const std::string &filter) {
  if (filter.empty())
    return false;
  for (size_t i = 0; i < filter.size(); i++) {
    char c = filter[i];
    if (c == '\0')
      return false;
    bool level_start = (i == 0 || filter[i - 1] == '/');
    bool level_end = (i + 1 == filter.size() || filter[i + 1] == '/');
    if (c == '+' && !(level_start && level_end))
      return false;
    if (c == '#' && !(level_start && i + 1 == filter.size()))
      return false;
  }
  return true;
}

bool Broker::topic_matches(const std::string &f, const std::string &t) {
  // Topics starting with '$' are never matched by a filter starting with a wildcard
  if (!t.empty() && t[0] == '$' && !f.empty() && (f[0] == '+' || f[0] == '#'))
    return false;

  size_t fi = 0, ti = 0;
  while (true) {
    size_t fe = f.find('/', fi);
    bool f_last = (fe == std::string::npos);
    if (f_last)
      fe = f.size();
    size_t te = t.find('/', ti);
    bool t_last = (te == std::string::npos);
    if (t_last)
      te = t.size();

    size_t flen = fe - fi;
    if (flen == 1 && f[fi] == '#')
      return true;  // "a/#" also matches "a"
    bool level_ok = (flen == 1 && f[fi] == '+') || (flen == te - ti && f.compare(fi, flen, t, ti, flen) == 0);
    if (!level_ok)
      return false;

    if (f_last && t_last)
      return true;
    if (f_last || t_last) {
      // topic ended but the filter goes on: only a trailing "/#" still matches ("a/#" vs "a")
      if (t_last)
        return f.compare(fe + 1, std::string::npos, "#") == 0;
      return false;
    }
    fi = fe + 1;
    ti = te + 1;
  }
}

// ---------------------------------------------------------------------------------------------------------------
// Sessions
// ---------------------------------------------------------------------------------------------------------------

void Broker::log_(LogLevel level, const char *fmt, ...) {
  if (this->listener_ == nullptr)
    return;
  char buf[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  this->listener_->on_log(level, buf);
}

Session *Broker::create_session(uint32_t now_ms, const std::string &peer) {
  auto s = std::make_unique<Session>();
  s->id = this->next_id_++;
  s->peer = peer;
  s->last_rx_ms = now_ms;
  Session *raw = s.get();
  this->sessions_.push_back(std::move(s));
  return raw;
}

void Broker::remove_session(Session *session) {
  auto it = std::find_if(this->sessions_.begin(), this->sessions_.end(),
                         [session](const std::unique_ptr<Session> &p) { return p.get() == session; });
  if (it == this->sessions_.end())
    return;

  Session &s = *session;
  bool was_connected = s.connected;
  s.dead = true;  // not a delivery target any more
  if (was_connected && !s.graceful && s.has_will) {
    this->log_(LogLevel::TRAFFIC, "[%u] publishing will message on '%s' (%u bytes)", s.id, s.will_topic.c_str(),
               static_cast<unsigned>(s.will_payload.size()));
    this->route_(s.will_topic, s.will_payload.data(), s.will_payload.size());
    if (this->listener_ != nullptr)
      this->listener_->on_publish(s, s.will_topic, s.will_payload.data(), s.will_payload.size(), s.will_qos, false);
  }
  if (was_connected && this->listener_ != nullptr)
    this->listener_->on_disconnect(s);
  this->log_(LogLevel::TRAFFIC, "[%u] closed (%s) client_id='%s'", s.id, s.graceful ? "DISCONNECT" : "connection lost",
             s.client_id.c_str());
  this->sessions_.erase(it);
}

size_t Broker::connected_count() const {
  size_t n = 0;
  for (const auto &s : this->sessions_) {
    if (s->connected && !s->dead)
      n++;
  }
  return n;
}

void Broker::tick(uint32_t now_ms) {
  for (auto &sp : this->sessions_) {
    Session &s = *sp;
    if (s.dead)
      continue;
    uint32_t idle = now_ms - s.last_rx_ms;
    if (!s.connected) {
      if (idle > this->options_.connect_timeout_ms) {
        this->log_(LogLevel::WARN, "[%u] %s sent no CONNECT within %u ms, dropping", s.id, s.peer.c_str(),
                   static_cast<unsigned>(this->options_.connect_timeout_ms));
        s.dead = true;
      }
    } else if (s.keepalive_s > 0 && idle > static_cast<uint32_t>(s.keepalive_s) * 1500u) {
      this->log_(LogLevel::WARN, "[%u] client_id='%s' keep-alive (%us) expired, dropping", s.id, s.client_id.c_str(),
                 static_cast<unsigned>(s.keepalive_s));
      s.dead = true;
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------------------------------------------

void Broker::queue_(Session &s, const std::vector<uint8_t> &bytes) {
  if (s.dead)
    return;
  if (s.tx_pending() + bytes.size() > this->options_.max_tx_backlog) {
    this->log_(LogLevel::WARN, "[%u] client_id='%s' does not read its data (backlog > %u bytes), dropping", s.id,
               s.client_id.c_str(), static_cast<unsigned>(this->options_.max_tx_backlog));
    s.dead = true;
    return;
  }
  s.tx.insert(s.tx.end(), bytes.begin(), bytes.end());
}

void Broker::connack_(Session &s, uint8_t v4_code, uint8_t v5_code) {
  std::vector<uint8_t> p;
  p.push_back(PKT_CONNACK << 4);
  if (s.level == 5) {
    p.push_back(3);
    p.push_back(0);  // no session present
    p.push_back(v5_code);
    p.push_back(0);  // no properties
  } else {
    p.push_back(2);
    p.push_back(0);
    p.push_back(v4_code);
  }
  this->queue_(s, p);
}

size_t Broker::route_(const std::string &topic, const uint8_t *payload, size_t len) {
  size_t delivered = 0;
  for (auto &sp : this->sessions_) {
    Session &s = *sp;
    if (!s.connected || s.dead)
      continue;
    bool match = false;
    for (const auto &sub : s.subs) {
      if (topic_matches(sub.filter, topic)) {
        match = true;
        break;
      }
    }
    if (!match)
      continue;

    // PUBLISH, QoS 0, no retain: [0x30][remaining length][topic][v5: property length 0][payload]
    std::vector<uint8_t> p;
    uint32_t body = 2u + static_cast<uint32_t>(topic.size()) + (s.level == 5 ? 1u : 0u) + static_cast<uint32_t>(len);
    p.reserve(body + 5);
    p.push_back(PKT_PUBLISH << 4);
    put_varint(p, body);
    put_u16(p, static_cast<uint16_t>(topic.size()));
    p.insert(p.end(), topic.begin(), topic.end());
    if (s.level == 5)
      p.push_back(0);
    p.insert(p.end(), payload, payload + len);
    this->queue_(s, p);
    delivered++;
  }
  return delivered;
}

size_t Broker::publish(const std::string &topic, const uint8_t *payload, size_t len) {
  return this->route_(topic, payload, len);
}

size_t Broker::subscriber_count(const std::string &topic) const {
  size_t n = 0;
  for (const auto &sp : this->sessions_) {
    if (!sp->connected || sp->dead)
      continue;
    for (const auto &sub : sp->subs) {
      if (topic_matches(sub.filter, topic)) {
        n++;
        break;
      }
    }
  }
  return n;
}

// ---------------------------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------------------------

bool Broker::feed(Session &s, const uint8_t *data, size_t len, uint32_t now_ms) {
  s.last_rx_ms = now_ms;

  // Discard the tail of an oversized PUBLISH
  if (s.skip > 0) {
    size_t drop = std::min(s.skip, len);
    s.skip -= drop;
    data += drop;
    len -= drop;
  }
  if (len > 0)
    s.rx.insert(s.rx.end(), data, data + len);

  size_t pos = 0;
  bool ok = true;
  while (ok && !s.dead) {
    size_t avail = s.rx.size() - pos;
    if (avail < 2)
      break;

    const uint8_t first = s.rx[pos];
    // Remaining length (variable, 1..4 bytes)
    uint32_t rem = 0, mul = 1;
    size_t hdr = 1;
    bool complete = false;
    for (int k = 0; k < 4; k++) {
      if (pos + hdr >= s.rx.size())
        break;
      uint8_t b = s.rx[pos + hdr];
      hdr++;
      rem += (b & 0x7F) * mul;
      mul *= 128;
      if (!(b & 0x80)) {
        complete = true;
        break;
      }
      if (k == 3) {  // 4 bytes and still a continuation bit
        this->log_(LogLevel::WARN, "[%u] malformed remaining length, dropping", s.id);
        ok = false;
      }
    }
    if (!ok)
      break;
    if (!complete)
      break;  // wait for more bytes

    const size_t total = hdr + rem;
    const uint8_t type = first >> 4;

    if (total > this->options_.max_packet_size) {
      if (type == PKT_PUBLISH) {
        // Oversized PUBLISH: consume what we have, drop the rest as it arrives
        this->log_(LogLevel::WARN,
                   "[%u] PUBLISH of %u bytes exceeds max_packet_size (%u): discarded (raise max_packet_size)", s.id,
                   static_cast<unsigned>(total), static_cast<unsigned>(this->options_.max_packet_size));
        size_t have = std::min(avail, total);
        pos += have;
        s.skip = total - have;
        continue;
      }
      this->log_(LogLevel::WARN, "[%u] packet type %u of %u bytes exceeds max_packet_size, dropping", s.id, type,
                 static_cast<unsigned>(total));
      ok = false;
      break;
    }
    if (avail < total)
      break;  // wait for the rest of the packet

    ok = this->handle_packet_(s, type, first & 0x0F, s.rx.data() + pos + hdr, rem, now_ms);
    pos += total;
  }

  if (pos > 0)
    s.rx.erase(s.rx.begin(), s.rx.begin() + static_cast<std::ptrdiff_t>(pos));
  if (!ok)
    s.close_after_flush = true;
  return ok && !s.dead;
}

bool Broker::handle_packet_(Session &s, uint8_t type, uint8_t flags, const uint8_t *body, size_t len,
                            uint32_t now_ms) {
  (void) now_ms;
  if (!s.connected) {
    if (type != PKT_CONNECT) {
      this->log_(LogLevel::WARN, "[%u] %s sent packet type %u before CONNECT, dropping", s.id, s.peer.c_str(), type);
      return false;
    }
    return this->handle_connect_(s, body, len);
  }

  switch (type) {
    case PKT_CONNECT:
      this->log_(LogLevel::WARN, "[%u] second CONNECT, dropping", s.id);
      return false;
    case PKT_PUBLISH:
      return this->handle_publish_(s, flags, body, len);
    case PKT_PUBACK:
    case PKT_PUBREC:
    case PKT_PUBCOMP:
      // Acknowledgements of messages we sent: we only ever send QoS 0, nothing to do
      return true;
    case PKT_PUBREL: {
      Reader r(body, len);
      uint16_t id = r.u16();
      if (!r.ok)
        return false;
      std::vector<uint8_t> p{static_cast<uint8_t>(PKT_PUBCOMP << 4), 2, static_cast<uint8_t>(id >> 8),
                             static_cast<uint8_t>(id & 0xFF)};
      this->queue_(s, p);
      return true;
    }
    case PKT_SUBSCRIBE:
      return this->handle_subscribe_(s, body, len);
    case PKT_UNSUBSCRIBE:
      return this->handle_unsubscribe_(s, body, len);
    case PKT_PINGREQ: {
      std::vector<uint8_t> p{static_cast<uint8_t>(PKT_PINGRESP << 4), 0};
      this->queue_(s, p);
      return true;
    }
    case PKT_DISCONNECT:
      s.graceful = true;
      this->log_(LogLevel::TRAFFIC, "[%u] DISCONNECT client_id='%s'", s.id, s.client_id.c_str());
      return false;
    default:
      this->log_(LogLevel::WARN, "[%u] unsupported packet type %u, dropping", s.id, type);
      return false;
  }
}

bool Broker::handle_connect_(Session &s, const uint8_t *body, size_t len) {
  Reader r(body, len);
  std::string proto;
  if (!r.str(proto))
    return false;
  uint8_t level = r.u8();
  uint8_t cflags = r.u8();
  uint16_t keepalive = r.u16();
  if (!r.ok)
    return false;

  const bool name_ok = (proto == "MQTT" && (level == 4 || level == 5)) || (proto == "MQIsdp" && level == 3);
  s.level = (level == 3 || level == 4 || level == 5) ? level : 4;
  if (!name_ok) {
    this->log_(LogLevel::WARN, "[%u] CONNECT with unsupported protocol '%s' level %u", s.id, proto.c_str(), level);
    this->connack_(s, 1, 0x84);
    return false;
  }
  if (cflags & 0x01)
    return false;  // reserved flag must be 0

  const bool clean = cflags & 0x02;
  const bool will = cflags & 0x04;
  const uint8_t will_qos = (cflags >> 3) & 0x03;
  const bool will_retain = cflags & 0x20;
  const bool has_pass = cflags & 0x40;
  const bool has_user = cflags & 0x80;

  if (s.level == 5 && !r.skip_props())
    return false;
  std::string client_id;
  if (!r.str(client_id))
    return false;

  std::string will_topic;
  std::vector<uint8_t> will_payload;
  if (will) {
    if (s.level == 5 && !r.skip_props())
      return false;
    if (!r.str(will_topic) || !r.bin(will_payload))
      return false;
  }
  std::string user, pass;
  if (has_user && !r.str(user))
    return false;
  if (has_pass) {
    std::vector<uint8_t> raw;
    if (!r.bin(raw))
      return false;
    pass.assign(raw.begin(), raw.end());
  }

  std::string will_info;
  if (will) {
    will_info = " (topic='" + will_topic + "' qos=" + std::to_string(will_qos) + " retain=" +
                std::to_string(will_retain ? 1 : 0) + " len=" + std::to_string(will_payload.size()) + ")";
  }
  this->log_(LogLevel::TRAFFIC,
             "[%u] CONNECT from %s: protocol=%s level=%u client_id='%s' keepalive=%us clean_session=%d will=%d%s "
             "user='%s' password=%s",
             s.id, s.peer.c_str(), proto.c_str(), level, client_id.c_str(), static_cast<unsigned>(keepalive), clean,
             will, will_info.c_str(), has_user ? user.c_str() : "", has_pass ? "yes" : "no");

  // Empty client identifier is only allowed with a clean session
  if (client_id.empty() && !clean) {
    this->connack_(s, 2, 0x85);
    return false;
  }
  if (!this->options_.username.empty()) {
    if (!has_user || user != this->options_.username || pass != this->options_.password) {
      this->log_(LogLevel::WARN,
                 "[%u] CONNECT refused: bad credentials (user='%s'). Use the broker username/password in the Jackery "
                 "app MQTT settings",
                 s.id, has_user ? user.c_str() : "");
      this->connack_(s, 4, 0x86);
      return false;
    }
  }

  // A second connection with the same client id replaces the first one
  for (auto &other : this->sessions_) {
    if (other.get() != &s && other->connected && !other->dead && other->client_id == client_id && !client_id.empty()) {
      this->log_(LogLevel::TRAFFIC, "[%u] client_id='%s' takes over session [%u]", s.id, client_id.c_str(), other->id);
      other->graceful = true;  // no will message for a takeover
      other->dead = true;
    }
  }

  s.client_id = client_id;
  s.keepalive_s = keepalive;
  s.has_will = will;
  s.will_topic = will_topic;
  s.will_payload = will_payload;
  s.will_qos = will_qos;
  s.connected = true;
  this->connack_(s, 0, 0x00);
  if (this->listener_ != nullptr)
    this->listener_->on_connect(s);
  return true;
}

bool Broker::handle_publish_(Session &s, uint8_t flags, const uint8_t *body, size_t len) {
  const bool dup = flags & 0x08;
  const uint8_t qos = (flags >> 1) & 0x03;
  const bool retain = flags & 0x01;
  if (qos == 3)
    return false;

  Reader r(body, len);
  std::string topic;
  if (!r.str(topic))
    return false;
  uint16_t id = 0;
  if (qos > 0)
    id = r.u16();
  if (s.level == 5 && !r.skip_props())
    return false;
  if (!r.ok)
    return false;
  if (!valid_topic_name(topic)) {
    this->log_(LogLevel::WARN, "[%u] PUBLISH with an invalid topic name, dropping", s.id);
    return false;
  }
  const uint8_t *payload = r.cur();
  const size_t plen = r.left();

  this->log_(LogLevel::TRAFFIC, "[%u] PUBLISH topic='%s' qos=%u retain=%d dup=%d len=%u payload=%s", s.id,
             topic.c_str(), qos, retain, dup, static_cast<unsigned>(plen), preview(payload, plen).c_str());

  if (qos == 1) {
    std::vector<uint8_t> p{static_cast<uint8_t>(PKT_PUBACK << 4), 2, static_cast<uint8_t>(id >> 8),
                           static_cast<uint8_t>(id & 0xFF)};
    this->queue_(s, p);
  } else if (qos == 2) {
    std::vector<uint8_t> p{static_cast<uint8_t>(PKT_PUBREC << 4), 2, static_cast<uint8_t>(id >> 8),
                           static_cast<uint8_t>(id & 0xFF)};
    this->queue_(s, p);
    if (dup)
      return true;  // retransmission of a QoS 2 message already delivered
  }

  this->route_(topic, payload, plen);
  if (this->listener_ != nullptr)
    this->listener_->on_publish(s, topic, payload, plen, qos, retain);
  return true;
}

bool Broker::handle_subscribe_(Session &s, const uint8_t *body, size_t len) {
  Reader r(body, len);
  uint16_t id = r.u16();
  if (s.level == 5 && !r.skip_props())
    return false;
  if (!r.ok || r.left() == 0)
    return false;

  std::vector<uint8_t> codes;
  struct Added {
    std::string filter;
    uint8_t qos;
  };
  std::vector<Added> added;
  while (r.left() > 0) {
    std::string filter;
    if (!r.str(filter))
      return false;
    uint8_t opt = r.u8();
    if (!r.ok)
      return false;
    uint8_t qos = opt & 0x03;
    if (!valid_filter(filter) || qos == 3) {
      this->log_(LogLevel::WARN, "[%u] SUBSCRIBE with an invalid filter '%s'", s.id, filter.c_str());
      codes.push_back(s.level == 5 ? 0x8F : 0x80);
      continue;
    }
    bool replaced = false;
    for (auto &sub : s.subs) {
      if (sub.filter == filter) {
        sub.qos = qos;
        replaced = true;
        break;
      }
    }
    if (!replaced)
      s.subs.push_back({filter, qos});
    codes.push_back(qos);  // granted QoS = requested (the broker itself only forwards at QoS 0, see header)
    added.push_back({filter, qos});
    this->log_(LogLevel::TRAFFIC, "[%u] SUBSCRIBE client_id='%s' filter='%s' qos=%u", s.id, s.client_id.c_str(),
               filter.c_str(), qos);
  }

  std::vector<uint8_t> p;
  p.push_back(PKT_SUBACK << 4);
  put_varint(p, 2u + (s.level == 5 ? 1u : 0u) + static_cast<uint32_t>(codes.size()));
  put_u16(p, id);
  if (s.level == 5)
    p.push_back(0);
  p.insert(p.end(), codes.begin(), codes.end());
  this->queue_(s, p);

  if (this->listener_ != nullptr) {
    for (const auto &a : added)
      this->listener_->on_subscribe(s, a.filter, a.qos);
  }
  return true;
}

bool Broker::handle_unsubscribe_(Session &s, const uint8_t *body, size_t len) {
  Reader r(body, len);
  uint16_t id = r.u16();
  if (s.level == 5 && !r.skip_props())
    return false;
  if (!r.ok || r.left() == 0)
    return false;

  std::vector<uint8_t> codes;
  while (r.left() > 0) {
    std::string filter;
    if (!r.str(filter))
      return false;
    auto it = std::find_if(s.subs.begin(), s.subs.end(), [&](const Subscription &x) { return x.filter == filter; });
    if (it != s.subs.end()) {
      s.subs.erase(it);
      codes.push_back(0x00);
    } else {
      codes.push_back(0x11);
    }
    this->log_(LogLevel::TRAFFIC, "[%u] UNSUBSCRIBE client_id='%s' filter='%s'", s.id, s.client_id.c_str(),
               filter.c_str());
  }

  std::vector<uint8_t> p;
  p.push_back(PKT_UNSUBACK << 4);
  if (s.level == 5) {
    put_varint(p, 3u + static_cast<uint32_t>(codes.size()));
    put_u16(p, id);
    p.push_back(0);
    p.insert(p.end(), codes.begin(), codes.end());
  } else {
    put_varint(p, 2);
    put_u16(p, id);
  }
  this->queue_(s, p);
  return true;
}

}  // namespace esphome::mqtt_broker
