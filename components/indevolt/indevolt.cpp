#include "indevolt.h"
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
namespace indevolt {

static const char *const TAG = "indevolt";

// Upper bound for a response (a GetData batch is a few hundred bytes, Sys.GetConfig a few kB).
static const size_t MAX_RESPONSE_BYTES = 12288;
// A power command identical to the previous one is not re-sent, except after this delay: it re-asserts the
// command in case the device dropped out of real-time control or the setting was changed from the app.
static const uint32_t POWER_REFRESH_MS = 60000;
// Consecutive failed polling cycles before the `online` binary sensor goes off.
static const uint8_t OFFLINE_AFTER_FAILURES = 3;
// A polling cycle that has not reported its last batch after this delay is considered lost.
static const uint32_t POLL_WATCHDOG_MS = 120000;
// Delay before the very first poll (the PollingComponent interval would otherwise delay it by one full period).
static const uint32_t FIRST_POLL_DELAY_MS = 5000;

// Points of the local API that the control logic relies on.
static const uint16_t POINT_WORK_MODE_READ = 7101;
static const uint16_t POINT_RT_COMMAND = 47015;  // [state, power, target SOC]
static const int32_t WORK_MODE_REALTIME = 4;
// Real-time command states.
static const int32_t RT_STANDBY = 0;
static const int32_t RT_CHARGING = 1;
static const int32_t RT_DISCHARGING = 2;

static const char *const PATH_GET_CONFIG = "/rpc/Sys.GetConfig";

// ---------------------------------------------------------------------------
// Request paths
//
// The official Home Assistant integration builds `config={"t":[...]}` / `config={"f":16,"t":..,"v":[..]}`
// without spaces and lets aiohttp percent-encode the URL; the same encoding ({ } " [ ] escaped, ':' and ','
// left alone) is produced here by hand.
// ---------------------------------------------------------------------------

// Default polling tier of a point (0 = fast, 1 = normal, 2 = slow). `poll:` on an entity overrides it.
//  - fast   (fast_poll_interval):   what drives automations -- power, state, SOC, work mode.
//  - slow   (slow_poll_interval):   near-static data -- identification, firmware, settings read-back.
//  - normal (normal_poll_interval): everything else -- voltages, currents, temperatures, energy counters, packs.
static const uint16_t FAST_POINTS[] = {
    6000,                    // battery power
    6001,                    // state
    6002,                    // battery SOC
    7101,                    // work mode
    1501,                    // DC (PV) output power
    1664, 1665, 1666, 1667,  // PV input power
    2101,                    // AC input power
    2108,                    // AC output power
    11016,                   // meter power (Gen2)
    21028,                   // meter power (Gen1)
    667,                     // bypass power
};

static const uint16_t SLOW_POINTS[] = {
    142,                                   // rated capacity
    6105,                                  // backup SOC / emergency power supply read-back
    606, 680, 2618, 7120, 7171,            // role, bypass, grid charging, load setting, light read-back
    11009, 11010, 11011,                   // inverter input / feed-in / max output limits read-back
    1109, 1118, 1119, 1120,                // firmwares
    1098, 1099, 1136, 1137, 1138, 1139,    // pack firmwares
    1140, 1141, 1142, 1143,
    9008, 9032, 9051, 9070, 9165, 9218,    // serial numbers
};

static uint8_t default_tier(uint16_t point) {
  for (uint16_t p : FAST_POINTS)
    if (p == point)
      return 0;
  for (uint16_t p : SLOW_POINTS)
    if (p == point)
      return 2;
  return 1;
}

static const char *const TIER_NAMES[] = {"fast", "normal", "slow"};

size_t IndevoltComponent::batch_count_(uint8_t tier) const {
  return (this->points_(tier).size() + this->batch_size_ - 1) / this->batch_size_;
}

std::string IndevoltComponent::get_data_path_(uint8_t tier, size_t batch) const {
  const auto &points = this->points_(tier);
  std::string path = "/rpc/Indevolt.GetData?config=%7B%22t%22:%5B";
  size_t begin = batch * this->batch_size_;
  size_t end = std::min(begin + (size_t) this->batch_size_, points.size());
  for (size_t i = begin; i < end; i++) {
    if (i > begin)
      path += ',';
    path += std::to_string(points[i]);
  }
  path += "%5D%7D";
  return path;
}

std::string IndevoltComponent::set_data_path_(uint16_t point, const std::vector<int32_t> &values) const {
  std::string path = "/rpc/Indevolt.SetData?config=%7B%22f%22:16,%22t%22:";
  path += std::to_string(point);
  path += ",%22v%22:%5B";
  for (size_t i = 0; i < values.size(); i++) {
    if (i > 0)
      path += ',';
    path += std::to_string(values[i]);
  }
  path += "%5D%7D";
  return path;
}

void IndevoltComponent::register_point_(uint16_t point) {
  if (point == 0)
    return;
  if (std::find(this->read_points_.begin(), this->read_points_.end(), point) == this->read_points_.end())
    this->read_points_.push_back(point);
}

// ---------------------------------------------------------------------------
// Minimal HTTP/1.1 client (blocking; only ever called from the background task)
// ---------------------------------------------------------------------------

long IndevoltComponent::parse_content_length_(const std::string &raw, size_t header_end) {
  std::string head = raw.substr(0, header_end);
  std::transform(head.begin(), head.end(), head.begin(), [](unsigned char c) { return std::tolower(c); });
  size_t pos = head.find("content-length:");
  if (pos == std::string::npos)
    return -1;
  return strtol(head.c_str() + pos + 15, nullptr, 10);
}

bool IndevoltComponent::decode_chunked_(const std::string &in, std::string &out) {
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

bool IndevoltComponent::http_request_(bool post, const std::string &path, int &status, std::string &response_body) {
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
  tv.tv_sec = this->timeout_ms_ / 1000;
  tv.tv_usec = (this->timeout_ms_ % 1000) * 1000;
  ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  bool ok = true;
  if (::connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
    ESP_LOGW(TAG, "[%s] Connect to port %u failed", this->host_.c_str(), this->port_);
    ok = false;
  }
  ::freeaddrinfo(res);

  if (ok) {
    // "Connection: close": one transaction per connection. The POST calls carry everything in the query
    // string, so they have an empty body (Content-Length: 0), like the aiohttp client of the HA integration.
    std::string req;
    req.reserve(256 + path.size());
    req += post ? "POST " : "GET ";
    req += path;
    req += " HTTP/1.1\r\nHost: ";
    req += this->host_;
    req += ':';
    req += port_str;
    req += "\r\nAccept: application/json\r\nConnection: close\r\n";
    if (post)
      req += "Content-Length: 0\r\n";
    req += "\r\n";

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
    bool chunked = false;
    uint32_t start = millis();

    while (millis() - start < this->timeout_ms_ && raw.size() < MAX_RESPONSE_BYTES) {
      ssize_t n = ::recv(sock, buf, sizeof(buf), 0);
      if (n <= 0)
        break;  // peer closed, timeout or error
      raw.append((const char *) buf, (size_t) n);

      if (header_end == std::string::npos) {
        size_t pos = raw.find("\r\n\r\n");
        if (pos != std::string::npos) {
          header_end = pos + 4;
          content_length = parse_content_length_(raw, header_end);
          std::string head = raw.substr(0, header_end);
          std::transform(head.begin(), head.end(), head.begin(), [](unsigned char c) { return std::tolower(c); });
          chunked = head.find("transfer-encoding: chunked") != std::string::npos;
        }
      }
      if (header_end != std::string::npos) {
        if (chunked) {
          // The terminating zero-size chunk ends the body.
          if (raw.size() >= header_end + 5 && raw.compare(raw.size() - 5, 5, "0\r\n\r\n") == 0)
            break;
        } else if (content_length >= 0 && raw.size() - header_end >= (size_t) content_length) {
          break;  // complete body received, no need to wait for the peer to close
        }
      }
    }

    if (header_end == std::string::npos || sscanf(raw.c_str(), "HTTP/%*d.%*d %d", &status) != 1) {
      ESP_LOGW(TAG, "[%s] No valid HTTP response (%u bytes)", this->host_.c_str(), (unsigned) raw.size());
      ok = false;
    } else if (chunked) {
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

  ::close(sock);
  return ok;
}

// ---------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------

void IndevoltComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up INDEVOLT %s (%s)...", this->gen1_ ? "BK1600" : "SolidFlex/PowerFlex 2000",
                this->host_.c_str());

  // The control logic needs the work mode, so it is always polled when auto_realtime_mode is on.
  if (this->auto_realtime_mode_)
    this->register_point_(POINT_WORK_MODE_READ);

  for (uint16_t p : this->read_points_) {
    uint8_t tier = default_tier(p);
    for (auto &o : this->tier_overrides_)
      if (o.first == p)
        tier = o.second;
    this->tier_points_[tier < TIERS ? tier : 1].push_back(p);
  }

  // One cycle in flight plus a few control writes queued behind it.
  this->job_queue_ = xQueueCreate(8, sizeof(IndevoltJob *));
  this->result_queue_ = xQueueCreate(16, sizeof(IndevoltResult *));
  if (this->job_queue_ == nullptr || this->result_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create job/result queues");
    this->mark_failed();
    return;
  }

  BaseType_t ok = xTaskCreate(&IndevoltComponent::task_trampoline_, "indevolt", 8192, this, 5, &this->task_handle_);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "Failed to create background task");
    this->mark_failed();
    return;
  }

  // Do not wait one full interval for the first values (fast list first, then normal, then slow).
  // The fast list runs on the PollingComponent interval (fast_poll_interval), the others on their own timers.
  this->set_timeout("first_poll", FIRST_POLL_DELAY_MS, [this]() { this->update(); });
  for (uint8_t t = 1; t < TIERS; t++) {
    if (this->tier_points_[t].empty())
      continue;
    static const char *const FIRST_NAMES[] = {"first_poll_fast", "first_poll_normal", "first_poll_slow"};
    static const char *const TIMER_NAMES[] = {"poll_fast", "poll_normal", "poll_slow"};
    this->set_timeout(FIRST_NAMES[t], FIRST_POLL_DELAY_MS + 500 * t, [this, t]() { this->start_poll_(t); });
    this->set_interval(TIMER_NAMES[t], this->tier_interval_ms_[t], [this, t]() { this->start_poll_(t); });
  }
}

void IndevoltComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "INDEVOLT local HTTP API:");
  ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Model generation: %s", this->gen1_ ? "Gen1 (BK1600 / BK1600 Ultra)" : "Gen2 (SolidFlex / PowerFlex 2000)");
  for (uint8_t t = 0; t < TIERS; t++) {
    ESP_LOGCONFIG(TAG, "  %s points (every %u s): %u in %u request(s) of up to %u", TIER_NAMES[t],
                  (unsigned) (t == 0 ? this->get_update_interval() : this->tier_interval_ms_[t]) / 1000,
                  (unsigned) this->tier_points_[t].size(), (unsigned) this->batch_count_(t), (unsigned) this->batch_size_);
  }
  ESP_LOGCONFIG(TAG, "  HTTP timeout: %u ms", (unsigned) this->timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Max charge power: %u W", this->max_charge_power_);
  ESP_LOGCONFIG(TAG, "  Max discharge power: %u W", this->max_discharge_power_);
  ESP_LOGCONFIG(TAG, "  Minimum power: %u W (below: standby)", this->min_power_);
  ESP_LOGCONFIG(TAG, "  Target SOC: charge %u %%, discharge %u %%", this->charge_target_soc_,
                this->discharge_target_soc_);
  ESP_LOGCONFIG(TAG, "  Auto real-time mode: %s", YESNO(this->auto_realtime_mode_));
  LOG_UPDATE_INTERVAL(this);
}

void IndevoltComponent::enqueue_info_() {
  if (this->info_pending_)
    return;
  auto *job = new IndevoltJob();
  job->type = JOB_INFO;
  if (xQueueSend(this->job_queue_, &job, 0) != pdTRUE) {
    delete job;
    return;
  }
  this->info_pending_ = true;
}

void IndevoltComponent::update() {
  if (this->job_queue_ == nullptr)
    return;
  // Identification (model / serial / firmware): retried on every cycle until it succeeds once.
  if (!this->info_done_)
    this->enqueue_info_();

  this->start_poll_(0);
}

void IndevoltComponent::start_poll_(uint8_t tier) {
  if (this->job_queue_ == nullptr || tier >= TIERS || this->points_(tier).empty())
    return;  // nothing to poll
  const int idx = tier;
  uint32_t now = millis();
  if (this->poll_pending_[idx]) {
    // Watchdog: a cycle that never reported back (result queue overflow, task stuck) must not block polling forever.
    if (now - this->poll_started_ms_[idx] < POLL_WATCHDOG_MS)
      return;  // the previous cycle has not finished yet
    ESP_LOGW(TAG, "[%s] Previous %s poll cycle did not finish in %u s, starting a new one", this->host_.c_str(),
             TIER_NAMES[tier], (unsigned) (POLL_WATCHDOG_MS / 1000));
  }
  auto *job = new IndevoltJob();
  job->type = JOB_POLL;
  job->tier = tier;
  if (xQueueSend(this->job_queue_, &job, 0) != pdTRUE) {
    delete job;
    ESP_LOGW(TAG, "[%s] Background task busy, skipping this %s poll cycle", this->host_.c_str(), TIER_NAMES[tier]);
    return;
  }
  this->poll_pending_[idx] = true;
  this->poll_started_ms_[idx] = now;
}

void IndevoltComponent::loop() {
  IndevoltResult *result = nullptr;
  while (xQueueReceive(this->result_queue_, &result, 0) == pdTRUE) {
    if (result != nullptr) {
      this->process_result_(result);
      delete result;
    }
  }
}

void IndevoltComponent::task_trampoline_(void *param) { static_cast<IndevoltComponent *>(param)->run_task_(); }

void IndevoltComponent::run_task_() {
  // Per list (0 fast, 1 normal, 2 slow): a cycle in progress and the next batch to request.
  bool polling[TIERS] = {false, false, false};
  size_t batch[TIERS] = {0, 0, 0};

  for (;;) {
    IndevoltJob *job = nullptr;
    // While a cycle is running, only peek at the queue: queued writes are served between two batches.
    TickType_t wait = (polling[0] || polling[1] || polling[2]) ? 0 : portMAX_DELAY;
    if (xQueueReceive(this->job_queue_, &job, wait) == pdTRUE) {
      if (job == nullptr)
        continue;
      if (job->type == JOB_POLL) {
        const int idx = job->tier < TIERS ? job->tier : 1;
        polling[idx] = true;
        batch[idx] = 0;
      } else {
        auto *result = new IndevoltResult();
        int status = 0;
        std::string body;
        [[maybe_unused]] uint32_t t0 = millis();
        if (job->type == JOB_WRITE) {
          bool ok = this->http_request_(true, job->path, status, body);
          result->type = RES_WRITE;
          result->is_power = job->is_power;
          result->success = ok && status >= 200 && status < 300;
        } else {
          bool ok = this->http_request_(false, PATH_GET_CONFIG, status, body);
          result->type = RES_INFO;
          result->success = ok && status == 200;
        }
        result->status = status;
        result->body = std::move(body);
        ESP_LOGV(TAG, "Background transaction (job %d) took %u ms", (int) job->type, (unsigned) (millis() - t0));
        if (xQueueSend(this->result_queue_, &result, 0) != pdTRUE)
          delete result;  // main loop fell behind: drop the result rather than block the network task
      }
      delete job;
      continue;  // look for more queued writes before the next batch
    }

    // Next batch: the fastest list in progress always goes first.
    int idx = polling[0] ? 0 : (polling[1] ? 1 : (polling[2] ? 2 : -1));
    if (idx >= 0) {
      const uint8_t tier = idx;
      const size_t total = this->batch_count_(tier);
      auto *result = new IndevoltResult();
      result->type = RES_BATCH;
      result->tier = tier;
      result->batch = batch[idx];
      int status = 0;
      std::string body;
      [[maybe_unused]] uint32_t t0 = millis();
      bool ok = this->http_request_(true, this->get_data_path_(tier, batch[idx]), status, body);
      result->status = status;
      result->success = ok && status == 200;
      if (result->success)
        result->body = std::move(body);
      ESP_LOGV(TAG, "GetData %s batch %u/%u took %u ms", TIER_NAMES[tier], (unsigned) (batch[idx] + 1),
               (unsigned) total, (unsigned) (millis() - t0));
      batch[idx]++;
      result->last = !result->success || batch[idx] >= total;
      if (result->last)
        polling[idx] = false;
      if (!result->success) {
        // The device is unreachable: abort the other list too (one timeout per failure, not one per list).
        for (int k = 0; k < TIERS; k++) {
          if (k != idx && polling[k]) {
            polling[k] = false;
            auto *abort_result = new IndevoltResult();
            abort_result->type = RES_BATCH;
            abort_result->tier = k;
            abort_result->success = false;
            abort_result->last = true;
            abort_result->aborted = true;
            abort_result->status = status;
            if (xQueueSend(this->result_queue_, &abort_result, pdMS_TO_TICKS(1000)) != pdTRUE)
              delete abort_result;
          }
        }
      }
      // Give the main loop a moment to drain: a lost batch result would leave the cycle unfinished
      // (the watchdog in start_poll_() recovers from that, but only after POLL_WATCHDOG_MS).
      if (xQueueSend(this->result_queue_, &result, pdMS_TO_TICKS(1000)) != pdTRUE)
        delete result;
    }
  }
}

// ---------------------------------------------------------------------------
// Result handling and parsing (main thread)
// ---------------------------------------------------------------------------

void IndevoltComponent::process_result_(IndevoltResult *result) {
  switch (result->type) {
    case RES_WRITE: {
      if (!result->success) {
        ESP_LOGW(TAG, "[%s] Write failed (HTTP status %d)", this->host_.c_str(), result->status);
        if (result->is_power)
          this->last_power_valid_ = false;  // force a re-send on the next request
        return;
      }
      // {"result": true} on success. Anything else explicit is reported, a missing field is not.
      json::parse_json(result->body, [this, result](JsonObject root) -> bool {
        JsonVariantConst r = JsonObjectConst(root)["result"];
        if (r.isNull())
          return true;
        bool accepted = r.is<bool>() ? r.as<bool>() : r.as<int>() != 0;
        if (!accepted) {
          ESP_LOGW(TAG, "[%s] The device refused a write: %s", this->host_.c_str(), result->body.c_str());
          if (result->is_power)
            this->last_power_valid_ = false;
        }
        return true;
      });
      return;
    }
    case RES_INFO: {
      this->info_pending_ = false;
      if (result->success)
        this->parse_info_(result->body);
      else
        ESP_LOGW(TAG, "[%s] Sys.GetConfig failed (HTTP status %d)", this->host_.c_str(), result->status);
      return;
    }
    case RES_BATCH:
      break;
  }

  if (result->aborted) {
    this->poll_pending_[result->tier < TIERS ? result->tier : 1] = false;
    return;
  }
  bool ok = result->success && this->parse_batch_(result->tier, result->batch, result->body);
  if (!ok) {
    ESP_LOGW(TAG, "[%s] %s poll failed (batch %u, HTTP status %d)", this->host_.c_str(),
             TIER_NAMES[result->tier < TIERS ? result->tier : 1],
             (unsigned) result->batch + 1, result->status);
    if (this->consecutive_failures_ < 255)
      this->consecutive_failures_++;
  } else if (result->last) {
    this->consecutive_failures_ = 0;
  }
  if (result->last)
    this->poll_pending_[result->tier < TIERS ? result->tier : 1] = false;  // a failed HTTP batch also ends the cycle on the task side

#ifdef USE_BINARY_SENSOR
  if (this->online_sensor_ != nullptr && result->last) {
    if (ok)
      this->online_sensor_->publish_state(true);
    else if (this->consecutive_failures_ >= OFFLINE_AFTER_FAILURES)
      this->online_sensor_->publish_state(false);
  }
#endif
}

// A point value as sent by the device: number, numeric string or boolean.
static bool to_number(JsonVariantConst v, float &out) {
  if (v.isNull())
    return false;
  if (v.is<const char *>()) {
    const char *s = v.as<const char *>();
    if (s == nullptr || *s == '\0')
      return false;
    char *end = nullptr;
    float f = strtof(s, &end);
    if (end == s)
      return false;
    out = f;
    return true;
  }
  if (v.is<bool>()) {
    out = v.as<bool>() ? 1.0f : 0.0f;
    return true;
  }
  if (v.is<float>()) {
    out = v.as<float>();
    return true;
  }
  return false;
}

static std::string format_firmware(const std::string &v) {
  if (v.size() == 5)
    return std::string(1, v[0]) + "." + v.substr(1, 2) + "." + v.substr(3, 2);
  if (v.size() == 3)
    return std::string(1, v[0]) + "." + v.substr(1, 2);
  return v;
}

static std::string number_to_text(float f) {
  char buf[24];
  if (f == std::floor(f) && std::fabs(f) < 1e9f)
    snprintf(buf, sizeof(buf), "%ld", (long) f);
  else
    snprintf(buf, sizeof(buf), "%g", f);
  return buf;
}

bool IndevoltComponent::parse_batch_(uint8_t tier, size_t batch, const std::string &body) {
  const std::vector<uint16_t> &points = this->points_(tier);
  size_t begin = batch * this->batch_size_;
  size_t end = std::min(begin + (size_t) this->batch_size_, points.size());

  auto in_batch = [&points, begin, end](uint16_t point) {
    for (size_t i = begin; i < end; i++)
      if (points[i] == point)
        return true;
    return false;
  };

  return json::parse_json(body, [this, &points, begin, end, &in_batch, &body](JsonObject root_obj) -> bool {
    JsonObjectConst root = root_obj;
    // Value of a point of this batch, or a null variant when the device did not report it.
    auto value_of = [&root](uint16_t point) -> JsonVariantConst {
      char key[8];
      snprintf(key, sizeof(key), "%u", (unsigned) point);
      return root[(const char *) key];
    };

    // Diagnostic aid: none of the requested points came back (wrong response layout, unknown points, ...).
    bool any = false;
    for (size_t i = begin; i < end; i++) {
      if (!value_of(points[i]).isNull()) {
        any = true;
        break;
      }
    }
    if (!any && this->empty_batch_warnings_ < 3) {
      this->empty_batch_warnings_++;
      ESP_LOGW(TAG, "[%s] None of the requested points %s..%s is in the response: %.160s", this->host_.c_str(),
               std::to_string(points[begin]).c_str(), std::to_string(points[end - 1]).c_str(),
               body.c_str());
    }

    float f;
    // Internal state used by the control logic.
    if (in_batch(POINT_WORK_MODE_READ) && to_number(value_of(POINT_WORK_MODE_READ), f))
      this->work_mode_ = (int32_t) f;

#ifdef USE_SENSOR
    for (auto &b : this->sensors_) {
      if (!in_batch(b.point) || !to_number(value_of(b.point), f))
        continue;
      b.sensor->publish_state(f * b.scale + b.offset);
    }
#endif

#ifdef USE_BINARY_SENSOR
    for (auto &b : this->binary_sensors_) {
      if (!in_batch(b.point) || !to_number(value_of(b.point), f))
        continue;
      b.sensor->publish_state((int32_t) f == b.on_value);
    }
#endif

#ifdef USE_TEXT_SENSOR
    for (auto &b : this->text_sensors_) {
      if (!in_batch(b.point))
        continue;
      JsonVariantConst v = value_of(b.point);
      if (v.isNull())
        continue;
      std::string text;
      float n = 0;
      bool numeric = to_number(v, n);
      if (b.conv == TEXT_RAW || b.conv == TEXT_FIRMWARE) {
        std::string raw;
        if (v.is<const char *>())
          raw = v.as<const char *>();
        else if (numeric)
          raw = number_to_text(n);
        else
          continue;
        text = b.conv == TEXT_FIRMWARE ? format_firmware(raw) : raw;
      } else {
        if (!numeric)
          continue;
        int32_t code = (int32_t) n;
        switch (b.conv) {
          case TEXT_WORK_MODE:
            text = code == 0   ? "Outdoor Portable"
                   : code == 1 ? "Self-consumed Prioritized"
                   : code == 4 ? "Real-Time Control"
                   : code == 5 ? "Charge/Discharge Schedule"
                               : "Unknown (" + std::to_string(code) + ")";
            break;
          case TEXT_STATE:
            text = code == 1000   ? "Static"
                   : code == 1001 ? "Charging"
                   : code == 1002 ? "Discharging"
                                  : "Unknown (" + std::to_string(code) + ")";
            break;
          case TEXT_ROLE:
            text = code == 1000   ? "Master"
                   : code == 1001 ? "Slave"
                   : code == 1002 ? "None"
                                  : "Unknown (" + std::to_string(code) + ")";
            break;
          default:
            text = std::to_string(code);
            break;
        }
      }
      if (b.sensor->state != text)  // versions and serial numbers rarely change: don't re-publish every poll
        b.sensor->publish_state(text);
    }
#endif

#ifdef USE_NUMBER
    for (auto &b : this->numbers_) {
      if (b.read_point == 0 || !in_batch(b.read_point) || !to_number(value_of(b.read_point), f))
        continue;
      b.number->publish_state(f);
    }
#endif

#ifdef USE_SWITCH
    for (auto &b : this->switches_) {
      if (b.read_point == 0 || !in_batch(b.read_point) || !to_number(value_of(b.read_point), f))
        continue;
      b.sw->publish_state((int32_t) f == b.on_value);
    }
#endif

#ifdef USE_SELECT
    for (auto &b : this->selects_) {
      if (b.read_point == 0 || !in_batch(b.read_point) || !to_number(value_of(b.read_point), f))
        continue;
      int32_t raw = (int32_t) f - b.read_offset;
      for (size_t i = 0; i < b.values.size(); i++) {
        if (b.values[i] == raw) {
          b.select->publish_state(i);
          break;
        }
      }
    }
#endif
    return true;
  });
}

void IndevoltComponent::parse_info_(const std::string &body) {
  // Sys.GetConfig is only used for identification; its full content is logged once to help diagnosing.
  ESP_LOGD(TAG, "[%s] Sys.GetConfig: %.400s", this->host_.c_str(), body.c_str());
  json::parse_json(body, [this](JsonObject root_obj) -> bool {
    JsonObjectConst device = JsonObjectConst(root_obj)["device"].as<JsonObjectConst>();
    if (device.isNull()) {
      ESP_LOGW(TAG, "[%s] Sys.GetConfig has no 'device' object", this->host_.c_str());
      return false;
    }
    auto str_of = [&device](const char *key) -> std::string {
      JsonVariantConst v = device[key];
      if (v.is<const char *>())
        return v.as<const char *>();
      float f;
      return to_number(v, f) ? number_to_text(f) : std::string();
    };
    std::string type = str_of("type");
    std::string sn = str_of("sn");
    std::string fw = str_of("f_ver");
    ESP_LOGI(TAG, "[%s] Device type: %s, serial number: %s, firmware: %s", this->host_.c_str(), type.c_str(),
             sn.c_str(), fw.c_str());

    // The generation drives which points are polled: warn when the configured model does not fit.
    bool device_is_gen1 = type.find("BK1600") != std::string::npos;
    bool device_is_gen2 = type.find("SF2000") != std::string::npos || type.find("PF2000") != std::string::npos;
    if ((device_is_gen1 && !this->gen1_) || (device_is_gen2 && this->gen1_))
      ESP_LOGW(TAG, "[%s] The device reports '%s' but the configured `model` is %s: check your YAML", this->host_.c_str(),
               type.c_str(), this->gen1_ ? "BK1600" : "SF2000");

#ifdef USE_TEXT_SENSOR
    for (auto &b : this->info_sensors_) {
      const std::string &text = b.kind == INFO_TYPE ? type : (b.kind == INFO_SERIAL ? sn : fw);
      if (!text.empty() && b.sensor->state != text)
        b.sensor->publish_state(text);
    }
#endif
    this->info_done_ = true;
    return true;
  });
}

// ---------------------------------------------------------------------------
// Control
// ---------------------------------------------------------------------------

bool IndevoltComponent::enqueue_write_(const std::string &path, bool is_power) {
  if (this->job_queue_ == nullptr)
    return false;
  auto *job = new IndevoltJob();
  job->type = JOB_WRITE;
  job->is_power = is_power;
  job->path = path;
  if (xQueueSend(this->job_queue_, &job, 0) != pdTRUE) {
    delete job;
    ESP_LOGW(TAG, "[%s] Background task busy, dropped write %s", this->host_.c_str(), path.c_str());
    return false;
  }
  ESP_LOGD(TAG, "[%s] Write queued: %s", this->host_.c_str(), path.c_str());
  return true;
}

void IndevoltComponent::write_point(uint16_t point, const std::vector<int32_t> &values) {
  this->enqueue_write_(this->set_data_path_(point, values), false);
}

int32_t IndevoltComponent::set_power(int32_t watts) {
  if (watts > (int32_t) this->max_discharge_power_)
    watts = this->max_discharge_power_;
  if (watts < -(int32_t) this->max_charge_power_)
    watts = -(int32_t) this->max_charge_power_;
  if (std::abs(watts) < (int32_t) this->min_power_ || watts == 0)
    watts = 0;

  this->requested_w_ = watts;
#ifdef USE_NUMBER
  this->publish_power_numbers_(watts);
#endif

  int32_t state = watts > 0 ? RT_DISCHARGING : (watts < 0 ? RT_CHARGING : RT_STANDBY);
  int32_t power = std::abs(watts);
  int32_t soc = watts < 0 ? this->charge_target_soc_ : this->discharge_target_soc_;

  std::vector<int32_t> command = {state, power, soc};
  std::string path = this->set_data_path_(POINT_RT_COMMAND, command);

  uint32_t now = millis();
  bool same = this->last_power_valid_ && path == this->last_power_command_ && now - this->last_power_ms_ < POWER_REFRESH_MS;
  // Real-time commands are ignored outside the Real-Time Control work mode (4).
  bool need_mode = this->auto_realtime_mode_ && this->work_mode_ != WORK_MODE_REALTIME;
  if (same && !need_mode)
    return watts;  // identical to the previous command, nothing to do

  if (need_mode) {
    this->write_point(POINT_WORK_MODE_WRITE, {WORK_MODE_REALTIME});
    this->work_mode_ = WORK_MODE_REALTIME;  // optimistic, corrected by the next poll
  }
  this->last_power_valid_ = this->enqueue_write_(path, true);
  if (this->last_power_valid_) {
    this->last_power_command_ = path;
    this->last_power_ms_ = now;
  }
  return watts;
}

#ifdef USE_NUMBER
// Keeps the power numbers consistent with the last request, whoever made it (a number or the outputs):
// the charging / discharging numbers show the magnitude of their own direction and 0 for the other one.
void IndevoltComponent::publish_power_numbers_(int32_t watts) {
  for (auto &b : this->numbers_) {
    if (b.kind == NUM_POWER_SETPOINT)
      b.number->publish_state((float) watts);
    else if (b.kind == NUM_CHARGE_POWER)
      b.number->publish_state(watts < 0 ? (float) -watts : 0.0f);
    else if (b.kind == NUM_DISCHARGE_POWER)
      b.number->publish_state(watts > 0 ? (float) watts : 0.0f);
  }
}
#endif

float IndevoltComponent::write_number(uint8_t kind, uint16_t write_point, float value) {
  switch (kind) {
    case NUM_POWER_SETPOINT: {
      value = std::min(std::max(value, -(float) this->max_charge_power_), (float) this->max_discharge_power_);
      return (float) this->set_power((int32_t) lroundf(value));
    }
    case NUM_CHARGE_POWER: {
      int32_t watts = (int32_t) lroundf(std::min(std::max(value, 0.0f), (float) this->max_charge_power_));
      // 0 W only stops a charge in progress: it must not cancel a discharge requested elsewhere.
      if (watts == 0 && this->requested_w_ >= 0)
        return 0.0f;
      int32_t effective = this->set_power(-watts);
      return effective < 0 ? (float) -effective : 0.0f;
    }
    case NUM_DISCHARGE_POWER: {
      int32_t watts = (int32_t) lroundf(std::min(std::max(value, 0.0f), (float) this->max_discharge_power_));
      if (watts == 0 && this->requested_w_ <= 0)
        return 0.0f;
      int32_t effective = this->set_power(watts);
      return effective > 0 ? (float) effective : 0.0f;
    }
    case NUM_REALTIME_POWER: {
      float max_power = (float) std::max(this->max_charge_power_, this->max_discharge_power_);
      value = std::min(std::max(value, 0.0f), max_power);
      this->write_point(write_point, {(int32_t) lroundf(value)});
      return value;
    }
    default:
      this->write_point(write_point, {(int32_t) lroundf(value)});
      return value;
  }
}

void IndevoltComponent::apply_output_requests_() {
  this->set_power((int32_t) lroundf(this->discharge_request_w_ - this->charge_request_w_));
}

void IndevoltComponent::set_charge_request(float fraction) {
  fraction = std::min(std::max(fraction, 0.0f), 1.0f);
  this->charge_request_w_ = fraction * (float) this->max_charge_power_;
  this->apply_output_requests_();
}

void IndevoltComponent::set_discharge_request(float fraction) {
  fraction = std::min(std::max(fraction, 0.0f), 1.0f);
  this->discharge_request_w_ = fraction * (float) this->max_discharge_power_;
  this->apply_output_requests_();
}

}  // namespace indevolt
}  // namespace esphome
