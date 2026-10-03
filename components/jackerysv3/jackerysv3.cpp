#include "jackerysv3.h"

#include <cstring>
#include <ctime>
#include <strings.h>

#include "esphome/components/json/json_util.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::jackerysv3 {

static const char *const TAG = "jackerysv3";

// Behaviour copied from the official Home Assistant integration
static constexpr uint32_t REAUTH_HINT_TIMEOUT_MS = 120000;  // nothing received this long after boot: token hint
static constexpr uint32_t HINT_REPEAT_MS = 600000;          // repeat the hint every 10 minutes
static constexpr uint32_t TOKEN_WARN_REPEAT_MS = 60000;
static constexpr uint32_t HOUSEKEEPING_MS = 1000;

enum BinarySource : uint8_t { SRC_STATE = 0, SRC_ONLINE = 1, SRC_CLIENT = 2 };

// ---------------------------------------------------------------------------------------------------------------
// Configuration / registration
// ---------------------------------------------------------------------------------------------------------------

void JackerySV3Hub::set_plug_sn(uint8_t index, const std::string &sn) {
  if (index < MAX_PLUGS)
    this->pinned_plugs_[index] = sn;
}

#ifdef USE_SENSOR
void JackerySV3Hub::add_sensor(const char *key, uint8_t index, sensor::Sensor *sensor) {
  SensorKind kind;
  if (!parse_sensor_kind(key, kind)) {
    ESP_LOGE(TAG, "Unknown sensor key '%s'", key);
    return;
  }
  this->sensors_.push_back({sensor, static_cast<uint8_t>(kind), index});
}
#endif

#ifdef USE_BINARY_SENSOR
void JackerySV3Hub::add_binary_sensor(const char *key, binary_sensor::BinarySensor *sensor) {
  if (strcmp(key, "online") == 0) {
    this->binary_sensors_.push_back({sensor, SRC_ONLINE, 0});
    return;
  }
  if (strcmp(key, "client_connected") == 0) {
    this->binary_sensors_.push_back({sensor, SRC_CLIENT, 0});
    return;
  }
  BinaryKind kind;
  if (!parse_binary_kind(key, kind)) {
    ESP_LOGE(TAG, "Unknown binary sensor key '%s'", key);
    return;
  }
  this->binary_sensors_.push_back({sensor, SRC_STATE, static_cast<uint8_t>(kind)});
}
#endif

#ifdef USE_TEXT_SENSOR
void JackerySV3Hub::add_text_sensor(const char *key, uint8_t index, text_sensor::TextSensor *sensor) {
  TextKind kind;
  if (!parse_text_kind(key, kind)) {
    ESP_LOGE(TAG, "Unknown text sensor key '%s'", key);
    return;
  }
  this->text_sensors_.push_back({sensor, static_cast<uint8_t>(kind), index, std::string(), false});
}
#endif

#ifdef USE_SWITCH
void JackerySV3Hub::add_switch(SwitchKind kind, uint8_t index, switch_::Switch *sw) {
  this->switches_.push_back({sw, static_cast<uint8_t>(kind), index});
}
#endif

#ifdef USE_NUMBER
void JackerySV3Hub::add_number(NumberKind kind, number::Number *number) {
  this->numbers_.push_back({number, static_cast<uint8_t>(kind), 0});
}
#endif

#ifdef USE_SELECT
void JackerySV3Hub::add_select(SelectKind kind, select::Select *select) {
  this->selects_.push_back({select, static_cast<uint8_t>(kind)});
}
#endif

// ---------------------------------------------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------------------------------------------

void JackerySV3Hub::setup() {
  this->state_ = std::make_unique<JackeryState>(this->sn_);
  for (uint8_t i = 0; i < MAX_PLUGS; i++) {
    if (!this->pinned_plugs_[i].empty())
      this->state_->set_plug_sn(i, this->pinned_plugs_[i]);
  }

  const std::string base = this->prefix_ + "/device/" + this->sn_;
  this->action_topic_ = base + "/action";
  this->status_topic_ = base + "/status";
  this->event_topic_ = base + "/event";

  this->start_ms_ = millis();
  if (this->broker_ != nullptr)
    this->broker_->add_sink(this);
}

void JackerySV3Hub::loop() {
  const uint32_t now = millis();

  if (this->poll_now_) {
    this->poll_now_ = false;
    this->send_polls_();
  }

  if (this->dirty_) {
    this->dirty_ = false;
    this->publish_states_(false);
  }

  if (now - this->last_housekeeping_ms_ >= HOUSEKEEPING_MS) {
    this->last_housekeeping_ms_ = now;
    this->housekeeping_(now);
  }
}

void JackerySV3Hub::update() {
  if (!this->ready_to_send_())
    return;
  this->send_polls_();
}

bool JackerySV3Hub::ready_to_send_() { return this->broker_ != nullptr && this->broker_->is_listening(); }

void JackerySV3Hub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Jackery SolarVault 3:\n"
                "  Serial number: %s\n"
                "  Topics: %s/device/%s/{status,event,action}\n"
                "  MQTT broker port: %u\n"
                "  Poll interval: %" PRIu32 " ms, offline timeout: %" PRIu32 " ms\n"
                "  Token: %s",
                this->sn_.c_str(), this->prefix_.c_str(), this->sn_.c_str(), static_cast<unsigned>(this->broker_ != nullptr ? this->broker_->port() : 0),
                this->get_update_interval(),
                this->offline_timeout_ms_, this->token_.empty() ? "(none)" : "set");
  for (uint8_t i = 0; i < MAX_PLUGS; i++) {
    if (!this->pinned_plugs_[i].empty())
      ESP_LOGCONFIG(TAG, "  Plug %u: %s", i, this->pinned_plugs_[i].c_str());
  }
#ifdef USE_SENSOR
  ESP_LOGCONFIG(TAG, "  Sensors: %u", static_cast<unsigned>(this->sensors_.size()));
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// Broker events
// ---------------------------------------------------------------------------------------------------------------

void JackerySV3Hub::on_message(const std::string &topic, const uint8_t *payload, size_t len) {
  if (strcasecmp(topic.c_str(), this->status_topic_.c_str()) != 0 &&
      strcasecmp(topic.c_str(), this->event_topic_.c_str()) != 0) {
    // another device / another hub sharing the broker
    return;
  }
  this->messages_++;
  const uint32_t now = millis();
  bool parsed = json::parse_json(payload, len, [&](JsonObject root) -> bool {
    JackeryState::Result r = this->state_->ingest(root, now);
    if (r.token_error) {
      this->token_error_ = true;
      if (now - this->last_token_warn_ms_ >= TOKEN_WARN_REPEAT_MS || this->last_token_warn_ms_ == 0) {
        this->last_token_warn_ms_ = now;
        ESP_LOGW(TAG,
                 "The battery rejected our token (type 123, errorCode 401). Check `token:` against the one shown "
                 "in the Jackery app.");
      }
    }
    ESP_LOGV(TAG, "Message on %s: recognized=%d main=%d sub=%d", topic.c_str(), r.recognized, r.main_updated,
             r.sub_updated);
    return true;
  });
  if (!parsed) {
    ESP_LOGW(TAG, "Invalid JSON on %s (%u bytes)", topic.c_str(), static_cast<unsigned>(len));
    return;
  }
  this->last_rx_ms_ = now;
  if (!this->ever_received_) {
    this->ever_received_ = true;
    ESP_LOGI(TAG, "First report received from the battery (%s)", this->sn_.c_str());
  }
  this->dirty_ = true;
}

void JackerySV3Hub::on_subscribed(const std::string &filter) {
  // The battery just subscribed to its action topic: poll it right away (from loop(), never from here)
  if (mqtt_broker::Broker::topic_matches(filter, this->action_topic_)) {
    ESP_LOGI(TAG, "The battery subscribed to %s", this->action_topic_.c_str());
    this->poll_now_ = true;
  }
}

void JackerySV3Hub::on_clients_changed(size_t connected) {
  this->clients_ = connected;
  this->dirty_ = true;
}

// ---------------------------------------------------------------------------------------------------------------
// Sending
// ---------------------------------------------------------------------------------------------------------------

std::string JackerySV3Hub::build_message_(int type, int event_id, bool always_token,
                                          const std::function<void(JsonObject)> &body) {
  return json::build_json([&](JsonObject root) {
    root["type"] = type;
    root["eventId"] = event_id;
    root["messageId"] = 1000 + random_uint32() % 9000;
    root["ts"] = static_cast<uint32_t>(::time(nullptr));
    if (always_token || !this->token_.empty())
      root["token"] = this->token_;
    if (body) {
      body(root["body"].to<JsonObject>());
    } else {
      root["body"] = nullptr;
    }
  });
}

bool JackerySV3Hub::publish_(const std::string &payload) {
  if (!this->ready_to_send_())
    return false;
  size_t delivered = this->broker_->publish(this->action_topic_, payload);
  if (delivered == 0) {
    if (!this->no_subscriber_logged_) {
      this->no_subscriber_logged_ = true;
      ESP_LOGW(TAG, "No MQTT client is subscribed to %s yet (%u client(s) connected): the battery is not reachable",
               this->action_topic_.c_str(), static_cast<unsigned>(this->broker_->connected_count()));
    }
    return false;
  }
  if (this->no_subscriber_logged_) {
    this->no_subscriber_logged_ = false;
    ESP_LOGI(TAG, "The battery is reachable on %s", this->action_topic_.c_str());
  }
  ESP_LOGV(TAG, "TX %s: %s", this->action_topic_.c_str(), payload.c_str());
  return true;
}

void JackerySV3Hub::send_polls_() {
  if (!this->ready_to_send_() || this->sn_.empty())
    return;
  // Same sequence as the Home Assistant integration: device status, system data, settings, CT and plug lists
  this->publish_(this->build_message_(25, 0, true, nullptr));
  this->publish_(this->build_message_(105, 0, true, nullptr));
  this->publish_(this->build_message_(2, 0, true, nullptr));
  for (int dev_type : {2, 6}) {
    this->publish_(this->build_message_(100, 0, true, [dev_type](JsonObject body) { body["devType"] = dev_type; }));
  }
}

bool JackerySV3Hub::send_main_command(const char *field, int value) {
  const std::string msg = this->build_message_(1, 3, false, [&](JsonObject body) {
    body["cmd"] = 5;
    body["rc"] = 1;
    body[field] = value;
  });
  ESP_LOGI(TAG, "Command %s=%d", field, value);
  return this->publish_(msg);
}

bool JackerySV3Hub::send_plug_command(uint8_t index, bool on) {
  const PlugState *plug = this->state_ ? this->state_->plug(index) : nullptr;
  if (plug == nullptr) {
    ESP_LOGW(TAG, "Plug %u has not been discovered yet, command ignored", index);
    return false;
  }
  if (plug->comm_mode != 1) {
    ESP_LOGW(TAG, "Plug %u (%s) is %s: only plugs with commMode 1 (local) can be controlled over MQTT", index,
             plug->sn.c_str(), plug->comm_mode == 2 ? "cloud-connected (commMode 2)" : "of unknown commMode");
    return false;
  }
  const std::string sn = plug->sn;
  const int dev_type = plug->dev_type;
  const std::string msg = this->build_message_(103, 0, false, [&](JsonObject body) {
    body["deviceSn"] = sn;
    body["devType"] = dev_type;
    body["sysSwitch"] = on ? 1 : 0;
  });
  ESP_LOGI(TAG, "Plug %u (%s) -> %s", index, sn.c_str(), on ? "ON" : "OFF");
  return this->publish_(msg);
}

// ---------------------------------------------------------------------------------------------------------------
// Housekeeping
// ---------------------------------------------------------------------------------------------------------------

void JackerySV3Hub::housekeeping_(uint32_t now) {
  const bool online = this->ever_received_ && (now - this->last_rx_ms_) <= this->offline_timeout_ms_;
  if (online != this->online_) {
    this->online_ = online;
    this->dirty_ = true;
    if (online)
      ESP_LOGI(TAG, "Battery %s is online", this->sn_.c_str());
    else
      ESP_LOGW(TAG, "Battery %s is offline (no report for %" PRIu32 " s)", this->sn_.c_str(),
               this->offline_timeout_ms_ / 1000);
  }

  // Never heard from the battery: most likely a wrong token / serial number / topic prefix, or the app is not
  // configured to use this broker
  if (!this->ever_received_ && now - this->start_ms_ > REAUTH_HINT_TIMEOUT_MS &&
      (this->last_hint_ms_ == 0 || now - this->last_hint_ms_ >= HINT_REPEAT_MS)) {
    this->last_hint_ms_ = now;
    ESP_LOGW(TAG,
             "No report received from the battery since boot (%u MQTT client(s) connected). Check the broker "
             "address / port in the Jackery app, the serial number (%s), `topic_prefix` (%s) and the token. "
             "Enable `log_traffic: true` on the mqtt_broker to see what the battery sends.",
             static_cast<unsigned>(this->clients_), this->sn_.c_str(), this->prefix_.c_str());
  }
}

// ---------------------------------------------------------------------------------------------------------------
// State publishing
// ---------------------------------------------------------------------------------------------------------------

void JackerySV3Hub::publish_states_(bool force) {
  if (!this->state_)
    return;
  const uint32_t now = millis();
  const JackeryState &st = *this->state_;
  const bool online = this->online_ || (this->ever_received_ && (now - this->last_rx_ms_) <= this->offline_timeout_ms_);
  const uint32_t timeout = this->offline_timeout_ms_;

#ifdef USE_SENSOR
  for (auto &e : this->sensors_) {
    const SensorKind kind = static_cast<SensorKind>(e.kind);
    float v = NAN;
    bool ok = online && st.sensor_value(kind, e.index, v);
    if (ok) {
      // Smart plug / CT values go stale on their own when that sub-device stops reporting
      if (kind == SensorKind::PLUG_POWER || kind == SensorKind::PLUG_ENERGY)
        ok = st.plug_fresh(e.index, now, timeout);
      else if (kind >= SensorKind::CT_FORWARD_POWER && kind <= SensorKind::CT_PHASE_C_REVERSE_POWER)
        ok = st.ct_fresh(now, timeout);
    }
    const float out = ok ? v : NAN;
    if (!ok && !e.has)
      continue;  // nothing published yet and nothing to publish
    const bool same = e.has && ((std::isnan(out) && std::isnan(e.last)) || out == e.last);
    if (same && !force)
      continue;
    e.entity->publish_state(out);
    e.last = out;
    e.has = true;
  }
#endif

#ifdef USE_BINARY_SENSOR
  for (auto &e : this->binary_sensors_) {
    bool value = false;
    bool ok = false;
    switch (e.source) {
      case SRC_ONLINE:
        value = online;
        ok = true;
        break;
      case SRC_CLIENT:
        value = this->clients_ > 0;
        ok = true;
        break;
      default:
        ok = online && st.binary_value(static_cast<BinaryKind>(e.kind), value);
        break;
    }
    if (!ok)
      continue;
    const int8_t v = value ? 1 : 0;
    if (v == e.last && !force)
      continue;
    e.entity->publish_state(value);
    e.last = v;
  }
#endif

#ifdef USE_TEXT_SENSOR
  for (auto &e : this->text_sensors_) {
    std::string s;
    if (!online || !st.text_value(static_cast<TextKind>(e.kind), e.index, s)) {
      if (static_cast<TextKind>(e.kind) == TextKind::STATUS && e.has && e.last != "Offline") {
        e.entity->publish_state("Offline");
        e.last = "Offline";
      }
      continue;
    }
    if (e.has && e.last == s && !force)
      continue;
    e.entity->publish_state(s);
    e.last = s;
    e.has = true;
  }
#endif

#ifdef USE_SWITCH
  for (auto &e : this->switches_) {
    bool v;
    if (!online || !st.switch_value(static_cast<SwitchKind>(e.kind), e.index, v))
      continue;
    if (static_cast<SwitchKind>(e.kind) == SwitchKind::PLUG && !st.plug_fresh(e.index, now, timeout))
      continue;
    const float f = v ? 1.0f : 0.0f;
    if (e.has && e.last == f && !force)
      continue;
    e.entity->publish_state(v);
    e.last = f;
    e.has = true;
  }
#endif

#ifdef USE_NUMBER
  for (auto &e : this->numbers_) {
    const NumberKind kind = static_cast<NumberKind>(e.kind);
    // Limits reported by the device (minSocChg, maxSocChg ...) narrow the slider
    float lo, hi;
    if (online && st.number_bounds(kind, lo, hi) && lo <= hi) {
      auto &t = e.entity->traits;
      if (t.get_min_value() != lo || t.get_max_value() != hi) {
        t.set_min_value(lo);
        t.set_max_value(hi);
      }
    }
    float v;
    if (!online || !st.number_value(kind, v)) {
      if (e.has && !std::isnan(e.last)) {
        e.entity->publish_state(NAN);
        e.last = NAN;
      }
      continue;
    }
    if (e.has && e.last == v && !force)
      continue;
    e.entity->publish_state(v);
    e.last = v;
    e.has = true;
  }
#endif

#ifdef USE_SELECT
  for (auto &e : this->selects_) {
    size_t idx;
    if (!online || !st.select_index(static_cast<SelectKind>(e.kind), idx))
      continue;
    if (idx >= e.entity->size())
      continue;
    const int8_t v = static_cast<int8_t>(idx);
    if (v == e.last && !force)
      continue;
    e.entity->publish_state(idx);
    e.last = v;
  }
#endif
}

}  // namespace esphome::jackerysv3
