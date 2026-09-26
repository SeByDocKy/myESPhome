#include "zensdk.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace esphome {
namespace zensdk {

static const char *const TAG = "zensdk";

// Total time budget for one HTTP transaction (connect + send + receive).
static const uint32_t HTTP_TIMEOUT_MS = 4000;
// Per-syscall socket timeout.
static const uint32_t SOCKET_TIMEOUT_MS = 2000;
// Upper bound for a response (a report with several battery packs is ~3 kB).
static const size_t MAX_RESPONSE_BYTES = 8192;
// A power command identical to the previous one is not re-sent, except after
// this delay: it re-asserts the command in case the device dropped out of smart
// mode or the setting was changed from the Zendure app in the meantime.
static const uint32_t POWER_REFRESH_MS = 60000;
// Consecutive failed polls before the `online` binary sensor goes off.
static const uint8_t OFFLINE_AFTER_FAILURES = 3;

static const char *const PATH_REPORT = "/properties/report";
static const char *const PATH_WRITE = "/properties/write";

// ---------------------------------------------------------------------------
// Minimal HTTP/1.0 client (blocking; only ever called from the background task)
// ---------------------------------------------------------------------------

long ZenSdkComponent::parse_content_length_(const std::string &raw, size_t header_end) {
  std::string head = raw.substr(0, header_end);
  std::transform(head.begin(), head.end(), head.begin(), [](unsigned char c) { return std::tolower(c); });
  size_t pos = head.find("content-length:");
  if (pos == std::string::npos)
    return -1;
  return strtol(head.c_str() + pos + 15, nullptr, 10);
}

bool ZenSdkComponent::decode_chunked_(const std::string &in, std::string &out) {
  size_t pos = 0;
  while (pos < in.size()) {
    size_t eol = in.find("\r\n", pos);
    if (eol == std::string::npos)
      return false;
    long size = strtol(in.c_str() + pos, nullptr, 16);
    pos = eol + 2;
    if (size <= 0)
      return true;
    if (pos + (size_t) size > in.size())
      return false;
    out.append(in, pos, (size_t) size);
    pos += (size_t) size + 2;
  }
  return true;
}

bool ZenSdkComponent::http_request_(bool post, const char *path, const std::string &body, int &status,
                                     std::string &response_body) {
  status = 0;
  response_body.clear();

  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;

  char port_str[6];
  snprintf(port_str, sizeof(port_str), "%u", this->port_);
  if (::getaddrinfo(this->host_.c_str(), port_str, &hints, &res) != 0 || res == nullptr) {
    ESP_LOGW(TAG, "[%s] Address resolution failed", this->host_.c_str());
    return false;
  }

  int sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sock < 0) {
    ESP_LOGW(TAG, "[%s] Failed to create socket", this->host_.c_str());
    ::freeaddrinfo(res);
    return false;
  }

  struct timeval tv{};
  tv.tv_sec = SOCKET_TIMEOUT_MS / 1000;
  tv.tv_usec = (SOCKET_TIMEOUT_MS % 1000) * 1000;
  ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  bool ok = true;
  if (::connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
    ESP_LOGW(TAG, "[%s] Connect to port %u failed", this->host_.c_str(), this->port_);
    ok = false;
  }
  ::freeaddrinfo(res);

  if (ok) {
    // HTTP/1.0 + "Connection: close": no keep-alive and no chunked encoding from a compliant server.
    std::string req;
    req.reserve(256 + body.size());
    req += post ? "POST " : "GET ";
    req += path;
    req += " HTTP/1.0\r\nHost: ";
    req += this->host_;
    req += "\r\nAccept: application/json\r\nConnection: close\r\n";
    if (post) {
      char header[64];
      snprintf(header, sizeof(header), "Content-Type: application/json\r\nContent-Length: %u\r\n",
               (unsigned) body.size());
      req += header;
    }
    req += "\r\n";
    if (post)
      req += body;

    size_t offset = 0;
    while (offset < req.size()) {
      ssize_t n = ::send(sock, req.data() + offset, req.size() - offset, 0);
      if (n <= 0) {
        ESP_LOGW(TAG, "[%s] Send failed", this->host_.c_str());
        ok = false;
        break;
      }
      offset += (size_t) n;
    }
  }

  if (ok) {
    std::string raw;
    uint8_t buf[512];
    size_t header_end = std::string::npos;
    long content_length = -1;
    uint32_t start = millis();

    while (millis() - start < HTTP_TIMEOUT_MS && raw.size() < MAX_RESPONSE_BYTES) {
      ssize_t n = ::recv(sock, buf, sizeof(buf), 0);
      if (n <= 0)
        break;  // peer closed, timeout or error
      raw.append((const char *) buf, (size_t) n);

      if (header_end == std::string::npos) {
        size_t pos = raw.find("\r\n\r\n");
        if (pos != std::string::npos) {
          header_end = pos + 4;
          content_length = parse_content_length_(raw, header_end);
        }
      }
      if (header_end != std::string::npos && content_length >= 0 &&
          raw.size() - header_end >= (size_t) content_length)
        break;  // complete body received, no need to wait for the peer to close
    }

    if (header_end == std::string::npos || sscanf(raw.c_str(), "HTTP/%*d.%*d %d", &status) != 1) {
      ESP_LOGW(TAG, "[%s] No valid HTTP response (%u bytes)", this->host_.c_str(), (unsigned) raw.size());
      ok = false;
    } else {
      std::string head = raw.substr(0, header_end);
      std::transform(head.begin(), head.end(), head.begin(), [](unsigned char c) { return std::tolower(c); });
      if (head.find("transfer-encoding: chunked") != std::string::npos) {
        if (!decode_chunked_(raw.substr(header_end), response_body)) {
          ESP_LOGW(TAG, "[%s] Malformed chunked response", this->host_.c_str());
          ok = false;
        }
      } else {
        response_body = raw.substr(header_end);
        if (content_length >= 0 && response_body.size() > (size_t) content_length)
          response_body.resize((size_t) content_length);
      }
    }
  }

  ::close(sock);
  return ok;
}

// ---------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------

void ZenSdkComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Zendure SolarFlow (%s)...", this->host_.c_str());

  // One poll in flight plus a few control writes queued behind it.
  this->job_queue_ = xQueueCreate(6, sizeof(ZenSdkJob *));
  this->result_queue_ = xQueueCreate(6, sizeof(ZenSdkResult *));
  if (this->job_queue_ == nullptr || this->result_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create job/result queues");
    this->mark_failed();
    return;
  }

  BaseType_t ok = xTaskCreate(&ZenSdkComponent::task_trampoline_, "zensdk", 8192, this, 5, &this->task_handle_);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "Failed to create background task");
    this->mark_failed();
  }
}

void ZenSdkComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Zendure SolarFlow (zenSDK local HTTP API):");
  ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Serial number: %s", this->sn_.c_str());
  ESP_LOGCONFIG(TAG, "  Max charge power: %u W", this->max_charge_power_);
  ESP_LOGCONFIG(TAG, "  Max discharge power: %u W", this->max_discharge_power_);
  ESP_LOGCONFIG(TAG, "  SoC scale: x%u", this->soc_scale_);
  ESP_LOGCONFIG(TAG, "  Write flash on stop: %s", YESNO(this->write_flash_on_stop_));
  LOG_UPDATE_INTERVAL(this);
}

void ZenSdkComponent::update() {
  auto *job = new ZenSdkJob();
  job->type = JOB_POLL;
  if (xQueueSend(this->job_queue_, &job, 0) != pdTRUE) {
    delete job;
    ESP_LOGW(TAG, "[%s] Background task busy, skipping this poll cycle", this->host_.c_str());
  }
}

void ZenSdkComponent::loop() {
  ZenSdkResult *result = nullptr;
  while (xQueueReceive(this->result_queue_, &result, 0) == pdTRUE) {
    if (result != nullptr) {
      this->process_result_(result);
      delete result;
    }
  }
}

void ZenSdkComponent::task_trampoline_(void *param) { static_cast<ZenSdkComponent *>(param)->run_task_(); }

void ZenSdkComponent::run_task_() {
  ZenSdkJob *job = nullptr;
  for (;;) {
    if (xQueueReceive(this->job_queue_, &job, portMAX_DELAY) != pdTRUE || job == nullptr)
      continue;

    auto *result = new ZenSdkResult();
    result->type = job->type;
    result->is_power = job->is_power;

    uint32_t t0 = millis();
    if (job->type == JOB_POLL) {
      int status = 0;
      std::string body;
      bool ok = this->http_request_(false, PATH_REPORT, "", status, body);
      result->status = status;
      result->success = ok && status == 200;
      if (result->success)
        result->body = std::move(body);
    } else {
      int status = 0;
      std::string body;
      bool ok = this->http_request_(true, PATH_WRITE, job->body, status, body);
      result->status = status;
      result->success = ok && status >= 200 && status < 300;
    }
    ESP_LOGV(TAG, "Background transaction (job %d) took %u ms", (int) job->type, (unsigned) (millis() - t0));
    delete job;
    job = nullptr;

    if (xQueueSend(this->result_queue_, &result, 0) != pdTRUE) {
      // Main loop fell behind: drop the result rather than block the network task.
      delete result;
    }
  }
}

// ---------------------------------------------------------------------------
// Result handling and report parsing (main thread)
// ---------------------------------------------------------------------------

void ZenSdkComponent::process_result_(ZenSdkResult *result) {
  if (result->type == JOB_WRITE) {
    if (!result->success) {
      ESP_LOGW(TAG, "[%s] Property write failed (HTTP status %d)", this->host_.c_str(), result->status);
      if (result->is_power)
        this->last_power_valid_ = false;  // force a re-send on the next request
    }
    return;
  }

  bool ok = result->success && this->parse_report_(result->body);
  if (!ok) {
    ESP_LOGW(TAG, "[%s] Poll failed (HTTP status %d)", this->host_.c_str(), result->status);
    if (this->consecutive_failures_ < 255)
      this->consecutive_failures_++;
  } else {
    this->consecutive_failures_ = 0;
  }
#ifdef USE_BINARY_SENSOR
  if (this->online_sensor_ != nullptr) {
    if (ok)
      this->online_sensor_->publish_state(true);
    else if (this->consecutive_failures_ >= OFFLINE_AFTER_FAILURES)
      this->online_sensor_->publish_state(false);
  }
#endif
}

// Looks up a property either in "properties" (pack < 0) or in packData[pack].
static JsonVariant get_value(JsonObject &props, JsonArray &packs, int8_t pack, const char *prop) {
  if (pack < 0)
    return props[prop].as<JsonVariant>();
  if ((size_t) pack >= packs.size())
    return JsonVariant();
  JsonObject pack_obj = packs[pack].as<JsonObject>();
  return pack_obj[prop].as<JsonVariant>();
}

bool ZenSdkComponent::parse_report_(const std::string &body) {
  return json::parse_json(body, [this](JsonObject root) -> bool {
    JsonObject props = root["properties"].as<JsonObject>();
    JsonArray packs = root["packData"].as<JsonArray>();
    if (props.isNull() && packs.isNull()) {
      ESP_LOGW(TAG, "[%s] Report contains neither 'properties' nor 'packData'", this->host_.c_str());
      return false;
    }

    // State the control path depends on, even when no entity is configured for it.
    JsonVariant grid_off = get_value(props, packs, -1, "gridOffPower");
    if (!grid_off.isNull())
      this->grid_off_power_ = grid_off.as<int>();

#ifdef USE_SENSOR
    for (auto &b : this->sensors_) {
      JsonVariant v = get_value(props, packs, b.pack, b.prop);
      if (v.isNull())
        continue;
      float raw = v.as<float>();
      float out;
      switch (b.conv) {
        case CONV_DECIKELVIN:
          if (raw == 0.0f)
            continue;  // 0 means "no reading", not -273 C
          out = (raw - 2731.0f) / 10.0f;
          break;
        case CONV_TEMP_AUTO:
          // The scale of hyperTmp is not documented by zenSDK, so detect it: ~3000 means 0.1 K,
          // ~300 means Kelvin, anything below that is taken as already being in Celsius.
          if (raw == 0.0f)
            continue;
          if (raw > 1000.0f)
            out = (raw - 2731.0f) / 10.0f;
          else if (raw > 200.0f)
            out = raw - 273.15f;
          else
            out = raw;
          break;
        case CONV_CELL_DELTA: {
          JsonVariant min_v = get_value(props, packs, b.pack, "minVol");
          if (min_v.isNull())
            continue;
          float min_raw = min_v.as<float>();
          if (raw == 0.0f || min_raw == 0.0f)
            continue;  // 0 means "no reading" (same rule as Zendure-HA)
          out = (raw - min_raw) * 0.01f;
          break;
        }
        case CONV_INT16:
          out = (float) (int16_t) (v.as<long>() & 0xFFFF) * b.scale;
          break;
        case CONV_VOLT_AUTO:
          out = raw > 200.0f ? raw * 0.01f : raw;
          break;
        default:
          out = raw * b.scale + b.offset;
          break;
      }
      b.sensor->publish_state(out);
    }
#endif

#ifdef USE_BINARY_SENSOR
    for (auto &b : this->binary_sensors_) {
      JsonVariant v = get_value(props, packs, -1, b.prop);
      if (!v.isNull())
        b.sensor->publish_state(v.as<int>() != 0);
    }
#endif

#ifdef USE_TEXT_SENSOR
    for (auto &b : this->text_sensors_) {
      JsonVariant v = get_value(props, packs, b.pack, b.prop);
      if (v.isNull())
        continue;
      std::string text;
      if (b.conv == TEXT_STATE) {
        switch (v.as<int>()) {
          case 0:
            text = "Standby";
            break;
          case 1:
            text = "Charging";
            break;
          case 2:
            text = "Discharging";
            break;
          default:
            text = "Unknown";
            break;
        }
      } else if (v.is<const char *>()) {
        text = v.as<const char *>();
      } else {
        text = std::to_string(v.as<long>());
      }
      b.sensor->publish_state(text);
    }
#endif

#ifdef USE_NUMBER
    for (auto &b : this->numbers_) {
      float value;
      if (b.kind == NUM_POWER_SETPOINT) {
        JsonVariant ac_mode = get_value(props, packs, -1, "acMode");
        JsonVariant in = get_value(props, packs, -1, "inputLimit");
        JsonVariant out = get_value(props, packs, -1, "outputLimit");
        if (ac_mode.isNull() || in.isNull() || out.isNull())
          continue;
        value = ac_mode.as<int>() == 2 ? out.as<float>() : -in.as<float>();
      } else {
        JsonVariant v = get_value(props, packs, -1, b.prop);
        if (v.isNull())
          continue;
        value = v.as<float>();
        if (b.kind == NUM_SOC)
          value /= (float) this->soc_scale_;
      }
      b.number->publish_state(value);
    }
#endif

#ifdef USE_SWITCH
    for (auto &b : this->switches_) {
      JsonVariant v = get_value(props, packs, -1, b.prop);
      if (!v.isNull())
        b.sw->publish_state(v.as<int>() != 0);
    }
#endif

#ifdef USE_SELECT
    for (auto &b : this->selects_) {
      JsonVariant v = get_value(props, packs, -1, b.prop);
      if (v.isNull())
        continue;
      int32_t index = v.as<int>() - b.base;
      if (index >= 0 && b.select->has_index((size_t) index))
        b.select->publish_state((size_t) index);
    }
#endif
    return true;
  });
}

// ---------------------------------------------------------------------------
// Control
// ---------------------------------------------------------------------------

bool ZenSdkComponent::enqueue_write_(const std::string &props, bool is_power) {
  // The device's local API requires the serial number in every POST body.
  this->http_id_++;
  char head[96];
  snprintf(head, sizeof(head), "{\"sn\":\"%s\",\"id\":%u,\"properties\":", this->sn_.c_str(), (unsigned) this->http_id_);

  auto *job = new ZenSdkJob();
  job->type = JOB_WRITE;
  job->is_power = is_power;
  job->body = std::string(head) + props + "}";

  if (xQueueSend(this->job_queue_, &job, 0) != pdTRUE) {
    delete job;
    ESP_LOGW(TAG, "[%s] Background task busy, dropped write %s", this->host_.c_str(), props.c_str());
    return false;
  }
  ESP_LOGD(TAG, "[%s] Write queued: %s", this->host_.c_str(), props.c_str());
  return true;
}

void ZenSdkComponent::write_property(const char *prop, int32_t raw_value) {
  char props[64];
  snprintf(props, sizeof(props), "{\"%s\":%d}", prop, (int) raw_value);
  this->enqueue_write_(props, false);
}

void ZenSdkComponent::set_power(int32_t watts) {
  if (watts > (int32_t) this->max_discharge_power_)
    watts = this->max_discharge_power_;
  if (watts < -(int32_t) this->max_charge_power_)
    watts = -(int32_t) this->max_charge_power_;

  char props[112];
  if (watts > 0) {
    snprintf(props, sizeof(props), "{\"smartMode\":1,\"acMode\":2,\"outputLimit\":%d,\"inputLimit\":0}", (int) watts);
  } else if (watts < 0) {
    snprintf(props, sizeof(props), "{\"smartMode\":1,\"acMode\":1,\"outputLimit\":0,\"inputLimit\":%d}", (int) -watts);
  } else {
    // smartMode 1 keeps the zero limits out of flash. Writing smartMode 0 on stop (what the Home Assistant
    // integration does when not off-grid) persists them, at the price of a flash write per stop.
    int smart = (this->write_flash_on_stop_ && this->grid_off_power_ == 0) ? 0 : 1;
    snprintf(props, sizeof(props), "{\"smartMode\":%d,\"acMode\":2,\"outputLimit\":0,\"inputLimit\":0}", smart);
  }

  std::string command(props);
  uint32_t now = millis();
  if (this->last_power_valid_ && command == this->last_power_props_ && now - this->last_power_ms_ < POWER_REFRESH_MS)
    return;  // identical to the previous command, nothing to do

  this->last_power_valid_ = this->enqueue_write_(command, true);
  if (this->last_power_valid_) {
    this->last_power_props_ = command;
    this->last_power_ms_ = now;
  }
}

float ZenSdkComponent::write_number(uint8_t kind, const char *prop, float value) {
  switch (kind) {
    case NUM_INPUT_LIMIT: {
      value = std::min(std::max(value, 0.0f), (float) this->max_charge_power_);
      this->set_power(-(int32_t) lroundf(value));
      return value;
    }
    case NUM_OUTPUT_LIMIT: {
      value = std::min(std::max(value, 0.0f), (float) this->max_discharge_power_);
      this->set_power((int32_t) lroundf(value));
      return value;
    }
    case NUM_POWER_SETPOINT: {
      value = std::min(std::max(value, -(float) this->max_charge_power_), (float) this->max_discharge_power_);
      this->set_power((int32_t) lroundf(value));
      return value;
    }
    case NUM_SOC:
      this->write_property(prop, (int32_t) lroundf(value * (float) this->soc_scale_));
      return value;
    default:
      this->write_property(prop, (int32_t) lroundf(value));
      return value;
  }
}

void ZenSdkComponent::apply_output_requests_() {
  this->set_power((int32_t) lroundf(this->discharge_request_w_ - this->charge_request_w_));
}

void ZenSdkComponent::set_charge_request(float fraction) {
  fraction = std::min(std::max(fraction, 0.0f), 1.0f);
  this->charge_request_w_ = fraction * (float) this->max_charge_power_;
  this->apply_output_requests_();
}

void ZenSdkComponent::set_discharge_request(float fraction) {
  fraction = std::min(std::max(fraction, 0.0f), 1.0f);
  this->discharge_request_w_ = fraction * (float) this->max_discharge_power_;
  this->apply_output_requests_();
}

}  // namespace zensdk
}  // namespace esphome
