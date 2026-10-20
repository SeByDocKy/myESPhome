#include "faikin.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace faikin {

static const char *const TAG = "faikin";

namespace {

constexpr uint8_t NAK_BUDGET_INITIAL = 2;   // refusals tolerated before a command is considered unsupported
constexpr uint8_t NAK_BUDGET_OK = 100;      // restored after every successful answer
constexpr uint8_t MAX_ERRORS = 3;           // consecutive failures before the unit is declared offline
constexpr uint32_t OFFLINE_RETRY_MS = 2000;

// Indices into the poll table below.
enum PollIndex : uint8_t { P_F1 = 0, P_F5, P_F6, P_F7, P_F9, P_RH, P_RI, P_Ra, P_RL, P_Rd };

struct CmdDef {
  uint8_t c1;
  uint8_t c2;
  uint8_t plen;
  uint8_t payload[2];
};

// Regular polling cycle. F9 (coarse temperatures) is only a fallback for units that refuse RH / Ra.
const CmdDef POLL_TABLE[FaikinComponent::POLL_COUNT] = {
    {'F', '1', 0, {0, 0}},  // power, mode, target temperature, fan
    {'F', '5', 0, {0, 0}},  // swing
    {'F', '6', 0, {0, 0}},  // powerful, comfort, quiet, streamer, sensor LED
    {'F', '7', 0, {0, 0}},  // econo, demand control
    {'F', '9', 0, {0, 0}},  // coarse room / outside temperature (fallback)
    {'R', 'H', 0, {0, 0}},  // room temperature
    {'R', 'I', 0, {0, 0}},  // liquid / coil temperature
    {'R', 'a', 0, {0, 0}},  // outside temperature
    {'R', 'L', 0, {0, 0}},  // indoor fan speed
    {'R', 'd', 0, {0, 0}},  // compressor
};

// Identification and capability queries, run once after the first successful connection.
const CmdDef PROBE_TABLE[FaikinComponent::PROBE_COUNT] = {
    {'F', '8', 0, {0, 0}},          // legacy protocol version
    {'F', 'Y', 2, {'0', '0'}},      // protocol version (v3+)
    {'F', 'C', 0, {0, 0}},          // model code
    {'F', 'K', 0, {0, 0}},          // optional features
    {'F', 'U', 2, {'0', '5'}},      // model name (v3.40+)
    {'F', '7', 0, {0, 0}},          // econo and demand control, raw
    {'F', '6', 0, {0, 0}},          // special modes, raw
};

uint8_t poll_index_for_group(uint8_t group) {
  switch (group) {
    case s21::GROUP_D5:
      return P_F5;
    case s21::GROUP_D6:
      return P_F6;
    case s21::GROUP_D7:
      return P_F7;
    default:
      return P_F1;
  }
}

std::string to_hex(const uint8_t *p, size_t n) {
  std::string s;
  char buf[4];
  for (size_t i = 0; i < n; i++) {
    snprintf(buf, sizeof(buf), "%02X", p[i]);
    s += buf;
    if (i + 1 < n)
      s += ' ';
  }
  return s;
}

std::string to_text(const uint8_t *p, size_t n) {
  std::string s;
  for (size_t i = 0; i < n; i++)
    s += (p[i] >= 0x20 && p[i] < 0x7F) ? static_cast<char>(p[i]) : '.';
  return s;
}

#ifdef USE_SENSOR
void pub_sensor(sensor::Sensor *s, float v) {
  if (s == nullptr || std::isnan(v))
    return;
  if (!s->has_state() || s->state != v)
    s->publish_state(v);
}
#endif

}  // namespace

// ---------------------------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------------------------

void FaikinComponent::setup() {
  for (size_t i = 0; i < POLL_COUNT; i++)
    this->nak_budget_[i] = NAK_BUDGET_INITIAL;
  this->next_start_ = millis() + 500;  // let the unit settle after boot
}

void FaikinComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Faikin (Daikin S21):\n"
                "  Update interval: %" PRIu32 " ms\n"
                "  Response timeout: %" PRIu32 " ms\n"
                "  Command gap: %" PRIu32 " ms\n"
                "  Boot probe: %s",
                this->update_interval_ms_, this->response_timeout_ms_, this->command_gap_ms_,
                YESNO(this->probe_enabled_));
  ESP_LOGCONFIG(TAG, "  Connected: %s", YESNO(this->online_));
  if (this->online_) {
    for (size_t i = 0; i < POLL_COUNT; i++) {
      ESP_LOGCONFIG(TAG, "  %c%c: %s", POLL_TABLE[i].c1, POLL_TABLE[i].c2,
                    this->nak_budget_[i] == 0 ? "not supported" : (this->poll_ok_[i] ? "ok" : "unknown"));
    }
  }
}

void FaikinComponent::loop() {
  const uint32_t now = millis();

  while (this->available()) {
    uint8_t b;
    if (!this->read_byte(&b))
      break;
    this->on_byte_(b, now);
  }

  if (this->phase_ != Phase::IDLE) {
    if (static_cast<int32_t>(now - this->deadline_) >= 0)
      this->finish_error_("timeout", now);
    return;
  }
  if (static_cast<int32_t>(now - this->next_start_) >= 0)
    this->start_next_(now);
}

// ---------------------------------------------------------------------------------------------
// Transaction engine
// ---------------------------------------------------------------------------------------------

bool FaikinComponent::poll_enabled_(size_t index) const {
  if (this->nak_budget_[index] == 0)
    return false;
  if (index == P_F9 && this->poll_ok_[P_RH] && this->poll_ok_[P_Ra])
    return false;
  return true;
}

bool FaikinComponent::start_next_(uint32_t now) {
  Txn &t = this->txn_;

  if (this->raw_pending_) {
    t = Txn{};
    t.kind = TxnKind::RAW;
    t.c1 = static_cast<uint8_t>(this->raw_cmd_[0]);
    t.c2 = static_cast<uint8_t>(this->raw_cmd_[1]);
    t.plen = static_cast<uint8_t>(this->raw_payload_.size());
    memcpy(t.payload, this->raw_payload_.data(), t.plen);
    t.expect_response = (t.c1 != 'D');
  } else if (this->online_ && this->dirty_ != 0) {
    // Pending writes first, one group per transaction.
    uint8_t group = 1;
    while ((this->dirty_ & group) == 0)
      group = static_cast<uint8_t>(group << 1);
    t = Txn{};
    t.kind = TxnKind::WRITE;
    t.group = group;
    t.c1 = 'D';
    t.plen = 4;
    t.expect_response = false;
    switch (group) {
      case s21::GROUP_D1:
        t.c2 = '1';
        s21::build_d1(this->desired_, t.payload);
        break;
      case s21::GROUP_D5:
        t.c2 = '5';
        s21::build_d5(this->desired_, t.payload);
        break;
      case s21::GROUP_D6:
        t.c2 = '6';
        s21::build_d6(this->desired_, t.payload);
        break;
      default:
        t.c2 = '7';
        s21::build_d7(this->desired_, t.payload);
        break;
    }
  } else if (this->online_ && this->refresh_mask_ != 0) {
    // Re-read what we just wrote.
    uint8_t idx = 0;
    while ((this->refresh_mask_ & (1u << idx)) == 0)
      idx++;
    this->refresh_mask_ &= static_cast<uint8_t>(~(1u << idx));
    t = Txn{};
    t.kind = TxnKind::POLL;
    t.index = idx;
    t.cycle = false;
    t.c1 = POLL_TABLE[idx].c1;
    t.c2 = POLL_TABLE[idx].c2;
  } else if (this->online_ && this->probe_pending_ && this->poll_index_ >= POLL_COUNT) {
    const CmdDef &def = PROBE_TABLE[this->probe_index_];
    t = Txn{};
    t.kind = TxnKind::PROBE;
    t.index = static_cast<uint8_t>(this->probe_index_);
    t.c1 = def.c1;
    t.c2 = def.c2;
    t.plen = def.plen;
    memcpy(t.payload, def.payload, def.plen);
  } else if (!this->online_) {
    // Not connected: keep asking for the basic status only.
    t = Txn{};
    t.kind = TxnKind::POLL;
    t.index = P_F1;
    t.cycle = false;
    t.c1 = POLL_TABLE[P_F1].c1;
    t.c2 = POLL_TABLE[P_F1].c2;
  } else {
    // Regular cycle.
    if (this->poll_index_ >= POLL_COUNT) {
      if (static_cast<int32_t>(now - this->cycle_start_) < static_cast<int32_t>(this->update_interval_ms_))
        return false;
      this->poll_index_ = 0;
      this->cycle_start_ = now;
    }
    while (this->poll_index_ < POLL_COUNT && !this->poll_enabled_(this->poll_index_))
      this->poll_index_++;
    if (this->poll_index_ >= POLL_COUNT)
      return false;
    t = Txn{};
    t.kind = TxnKind::POLL;
    t.index = static_cast<uint8_t>(this->poll_index_);
    t.cycle = true;
    t.c1 = POLL_TABLE[this->poll_index_].c1;
    t.c2 = POLL_TABLE[this->poll_index_].c2;
  }

  this->begin_txn_(now);
  return true;
}

void FaikinComponent::discard_rx_() {
  uint8_t b;
  while (this->available()) {
    if (!this->read_byte(&b))
      break;
  }
}

void FaikinComponent::begin_txn_(uint32_t now) {
  uint8_t frame[s21::MAX_FRAME];
  const size_t len = s21::build_frame(this->txn_.c1, this->txn_.c2, this->txn_.payload, this->txn_.plen, frame);
  if (len == 0) {
    ESP_LOGW(TAG, "Cannot build %c%c frame", this->txn_.c1, this->txn_.c2);
    this->raw_pending_ = false;
    return;
  }
  this->discard_rx_();
  if (this->txn_.plen != 0) {
    ESP_LOGD(TAG, "TX %c%c payload=[%s]", this->txn_.c1, this->txn_.c2,
             to_hex(this->txn_.payload, this->txn_.plen).c_str());
  } else {
    ESP_LOGV(TAG, "TX %c%c", this->txn_.c1, this->txn_.c2);
  }
  this->write_array(frame, len);
  this->parser_.reset();
  this->phase_ = Phase::WAIT_ACK;
  this->deadline_ = now + this->response_timeout_ms_;
}

void FaikinComponent::on_byte_(uint8_t b, uint32_t now) {
  switch (this->phase_) {
    case Phase::IDLE:
      return;  // stray byte, ignore

    case Phase::WAIT_ACK:
      if (b == s21::ACK) {
        if (!this->txn_.expect_response) {
          this->finish_ok_(now);
        } else {
          this->phase_ = Phase::WAIT_STX;
          this->deadline_ = now + this->response_timeout_ms_;
        }
      } else if (b == s21::NAK) {
        this->finish_nak_(now);
      } else if (b == s21::STX) {
        // Some units skip the ACK and answer straight away.
        this->parser_.reset();
        this->parser_.feed(b);
        this->phase_ = Phase::RECEIVING;
        this->deadline_ = now + this->response_timeout_ms_;
      } else {
        char msg[48];
        snprintf(msg, sizeof(msg), "unexpected byte 0x%02X instead of ACK", b);
        this->finish_error_(msg, now);
      }
      return;

    case Phase::WAIT_STX:
      if (b == s21::STX) {
        this->parser_.reset();
        this->parser_.feed(b);
        this->phase_ = Phase::RECEIVING;
        this->deadline_ = now + this->response_timeout_ms_;
      }
      return;

    case Phase::RECEIVING: {
      const s21::ParseStatus status = this->parser_.feed(b);
      if (status == s21::ParseStatus::COMPLETE)
        this->handle_frame_(now);
      else if (status == s21::ParseStatus::TOO_LONG)
        this->finish_error_("response too long", now);
      return;
    }
  }
}

void FaikinComponent::handle_frame_(uint32_t now) {
  // Always acknowledge, even a damaged frame: otherwise the unit keeps repeating it.
  this->write_byte(s21::ACK);

  const s21::FrameError err = this->parser_.validate();
  if (err != s21::FrameError::NONE) {
    this->finish_error_(err == s21::FrameError::BAD_CHECKSUM ? "bad checksum" : "frame too short", now);
    return;
  }
  const uint8_t c1 = this->parser_.cmd1();
  const uint8_t c2 = this->parser_.cmd2();
  if (c1 == this->txn_.c1) {
    this->finish_error_("our own request came back (loopback): check the wiring and the pin inversion", now);
    return;
  }
  // A response carries the first command character incremented by one and the second unchanged.
  if (c1 != this->txn_.c1 + 1 || c2 != this->txn_.c2) {
    this->finish_error_("unexpected response", now);
    return;
  }

  switch (this->txn_.kind) {
    case TxnKind::POLL:
      this->handle_poll_response_();
      break;
    case TxnKind::PROBE:
      this->handle_probe_response_();
      break;
    case TxnKind::RAW:
      ESP_LOGI(TAG, "RAW %c%c -> %c%c payload=[%s] text=\"%s\"", this->txn_.c1, this->txn_.c2, c1, c2,
               to_hex(this->parser_.payload(), this->parser_.payload_length()).c_str(),
               to_text(this->parser_.payload(), this->parser_.payload_length()).c_str());
      break;
    case TxnKind::WRITE:
      break;
  }
  this->finish_ok_(now);
}

void FaikinComponent::finish_ok_(uint32_t now) {
  this->phase_ = Phase::IDLE;
  this->next_start_ = now + this->command_gap_ms_;
  this->errors_ = 0;

  switch (this->txn_.kind) {
    case TxnKind::POLL: {
      const size_t i = this->txn_.index;
      this->poll_ok_[i] = true;
      this->nak_budget_[i] = NAK_BUDGET_OK;
      if (this->txn_.cycle)
        this->poll_index_++;
      if (i == P_F1 && !this->online_)
        this->set_online_(true, now);
      break;
    }
    case TxnKind::WRITE: {
      // Optimistic update: show the new value now, the follow-up read confirms (or reverts) it.
      const s21::State before = this->state_;
      s21::copy_groups(this->state_, this->desired_, this->txn_.group);
      this->dirty_ &= static_cast<uint8_t>(~this->txn_.group);
      this->refresh_mask_ |= static_cast<uint8_t>(1u << poll_index_for_group(this->txn_.group));
      this->publish_entities_();
      this->notify_if_changed_(before);
      break;
    }
    case TxnKind::PROBE:
      if (++this->probe_index_ >= PROBE_COUNT)
        this->finish_probe_();
      break;
    case TxnKind::RAW:
      if (!this->txn_.expect_response)
        ESP_LOGI(TAG, "RAW %c%c -> ACK", this->txn_.c1, this->txn_.c2);
      this->raw_pending_ = false;
      break;
  }
}

void FaikinComponent::finish_nak_(uint32_t now) {
  this->phase_ = Phase::IDLE;
  this->next_start_ = now + this->command_gap_ms_;
  this->errors_ = 0;  // the unit answered, so it is alive

  switch (this->txn_.kind) {
    case TxnKind::POLL: {
      const size_t i = this->txn_.index;
      this->poll_ok_[i] = false;
      if (this->nak_budget_[i] > 0)
        this->nak_budget_[i]--;
      if (this->nak_budget_[i] == 0 && this->online_)
        ESP_LOGW(TAG, "%c%c refused by the unit, no longer polled", this->txn_.c1, this->txn_.c2);
      else
        ESP_LOGD(TAG, "%c%c refused by the unit", this->txn_.c1, this->txn_.c2);
      if (this->txn_.cycle)
        this->poll_index_++;
      break;
    }
    case TxnKind::WRITE:
      ESP_LOGW(TAG, "D%c refused by the unit (NAK)", this->txn_.c2);
      this->dirty_ &= static_cast<uint8_t>(~this->txn_.group);
      s21::copy_groups(this->desired_, this->state_, this->txn_.group);
      break;
    case TxnKind::PROBE:
      ESP_LOGI(TAG, "probe %c%c: NAK (not supported by this unit)", this->txn_.c1, this->txn_.c2);
      if (++this->probe_index_ >= PROBE_COUNT)
        this->finish_probe_();
      break;
    case TxnKind::RAW:
      ESP_LOGI(TAG, "RAW %c%c -> NAK", this->txn_.c1, this->txn_.c2);
      this->raw_pending_ = false;
      break;
  }
}

void FaikinComponent::finish_error_(const char *why, uint32_t now) {
  this->phase_ = Phase::IDLE;
  this->discard_rx_();
  if (this->errors_ < 255)
    this->errors_++;
  if (this->errors_ <= MAX_ERRORS || this->errors_ % 30 == 0) {
    ESP_LOGW(TAG, "%c%c failed: %s (%u consecutive error(s))", this->txn_.c1, this->txn_.c2, why, this->errors_);
  } else {
    ESP_LOGV(TAG, "%c%c failed: %s", this->txn_.c1, this->txn_.c2, why);
  }
  this->next_start_ = now + (this->online_ ? this->command_gap_ms_ * 4 : OFFLINE_RETRY_MS);

  switch (this->txn_.kind) {
    case TxnKind::WRITE:
      // Do not keep a stale command around: drop it and fall back to the unit's own values.
      this->dirty_ &= static_cast<uint8_t>(~this->txn_.group);
      s21::copy_groups(this->desired_, this->state_, this->txn_.group);
      break;
    case TxnKind::PROBE:
      if (++this->probe_index_ >= PROBE_COUNT)
        this->finish_probe_();
      break;
    case TxnKind::RAW:
      this->raw_pending_ = false;
      break;
    case TxnKind::POLL:
      break;
  }

  if (this->online_ && this->errors_ >= MAX_ERRORS)
    this->set_online_(false, now);
}

void FaikinComponent::set_online_(bool online, uint32_t now) {
  if (this->online_ == online)
    return;
  this->online_ = online;
  if (online) {
    ESP_LOGI(TAG, "Unit is responding");
    this->poll_index_ = 0;
    this->cycle_start_ = now;
    if (this->probe_enabled_ && !this->probe_done_) {
      this->probe_pending_ = true;
      this->probe_index_ = 0;
    }
  } else {
    ESP_LOGW(TAG, "Lost communication with the unit");
    for (size_t i = 0; i < POLL_COUNT; i++) {
      this->nak_budget_[i] = NAK_BUDGET_INITIAL;
      this->poll_ok_[i] = false;
    }
    this->dirty_ = 0;
    this->refresh_mask_ = 0;
    this->poll_index_ = POLL_COUNT;
  }
#ifdef USE_BINARY_SENSOR
  if (this->connected_binary_sensor_ != nullptr)
    this->connected_binary_sensor_->publish_state(online);
#endif
}

// ---------------------------------------------------------------------------------------------
// Decoding and publishing
// ---------------------------------------------------------------------------------------------

void FaikinComponent::handle_poll_response_() {
  const s21::State before = this->state_;
  const uint8_t groups = s21::apply_response(this->state_, this->parser_.cmd1(), this->parser_.cmd2(),
                                             this->parser_.payload(), this->parser_.payload_length());
  if (groups == 0) {
    ESP_LOGD(TAG, "%c%c answer not understood (%u byte payload)", this->parser_.cmd1(), this->parser_.cmd2(),
             static_cast<unsigned>(this->parser_.payload_length()));
    return;
  }
  // Groups without a pending write follow the unit.
  s21::copy_groups(this->desired_, this->state_, static_cast<uint8_t>(groups & ~this->dirty_ & 0x0F));
  this->publish_entities_();
  this->notify_if_changed_(before);
}

void FaikinComponent::notify_if_changed_(const s21::State &before) {
  if (!s21::state_equal(before, this->state_))
    this->state_callback_.call();
}

void FaikinComponent::publish_entities_() {
  const s21::State &s = this->state_;
#ifdef USE_SENSOR
  pub_sensor(this->home_temperature_sensor_, s.home);
  pub_sensor(this->outside_temperature_sensor_, s.outside);
  pub_sensor(this->liquid_temperature_sensor_, s.liquid);
  pub_sensor(this->fan_speed_sensor_, s.fan_rpm >= 0 ? static_cast<float>(s.fan_rpm) : NAN);
  pub_sensor(this->compressor_sensor_, s.comp >= 0 ? static_cast<float>(s.comp) : NAN);
#endif
#ifdef USE_SWITCH
  if (s.known & s21::GROUP_D6) {
    if (this->powerful_switch_ != nullptr)
      this->powerful_switch_->publish_state(s.powerful);
    if (this->comfort_switch_ != nullptr)
      this->comfort_switch_->publish_state(s.comfort);
    if (this->quiet_switch_ != nullptr)
      this->quiet_switch_->publish_state(s.quiet);
    if (this->streamer_switch_ != nullptr)
      this->streamer_switch_->publish_state(s.streamer);
    if (this->sensor_switch_ != nullptr)
      this->sensor_switch_->publish_state(s.sensor_led);
  }
  if ((s.known & s21::GROUP_D7) && this->econo_switch_ != nullptr)
    this->econo_switch_->publish_state(s.econo);
#endif
#ifdef USE_NUMBER
  if ((s.known & s21::GROUP_D7) && s.demand >= 0 && this->demand_number_ != nullptr) {
    const float v = static_cast<float>(s.demand);
    if (!this->demand_number_->has_state() || this->demand_number_->state != v)
      this->demand_number_->publish_state(v);
  }
#endif
}

void FaikinComponent::handle_probe_response_() {
  const uint8_t *p = this->parser_.payload();
  const size_t n = this->parser_.payload_length();
  ESP_LOGI(TAG, "probe %c%c: payload=[%s] text=\"%s\"", this->txn_.c1, this->txn_.c2, to_hex(p, n).c_str(),
           to_text(p, n).c_str());
  switch (this->txn_.c2) {
    case '8':
      this->probe_f8_ = to_text(p, n);
      break;
    case 'Y': {
      char v[16];
      if (s21::format_protocol_version(p, n, v, sizeof(v))) {
        this->probe_version_ = v;
        this->probe_have_version_ = true;
      }
      break;
    }
    case 'C': {
      char r[s21::MAX_PAYLOAD + 1];
      s21::reverse_ascii(p, n, r);
      this->probe_model_ = r;
      break;
    }
    default:
      break;
  }
}

void FaikinComponent::finish_probe_() {
  this->probe_pending_ = false;
  this->probe_done_ = true;

  std::string version;
  if (this->probe_have_version_)
    version = this->probe_version_ + " (FY00)";
  else if (!this->probe_f8_.empty())
    version = "F8=" + this->probe_f8_;
  else
    version = "unversioned (F8 and FY00 refused)";

  ESP_LOGI(TAG, "Probe done: protocol %s, model code \"%s\"", version.c_str(), this->probe_model_.c_str());
#ifdef USE_TEXT_SENSOR
  if (this->protocol_version_text_sensor_ != nullptr)
    this->protocol_version_text_sensor_->publish_state(version);
  if (this->model_code_text_sensor_ != nullptr && !this->probe_model_.empty())
    this->model_code_text_sensor_->publish_state(this->probe_model_);
#endif
}

void FaikinComponent::run_probe() {
  this->probe_done_ = false;
  this->probe_have_version_ = false;
  this->probe_version_.clear();
  this->probe_f8_.clear();
  this->probe_model_.clear();
  this->probe_index_ = 0;
  this->probe_pending_ = true;
  ESP_LOGI(TAG, "Probe requested");
}

void FaikinComponent::send_raw(const std::string &command, const std::string &payload) {
  if (command.size() != 2) {
    ESP_LOGW(TAG, "send_raw: the command must be exactly two characters, got \"%s\"", command.c_str());
    return;
  }
  if (payload.size() > s21::MAX_PAYLOAD) {
    ESP_LOGW(TAG, "send_raw: payload too long (%u bytes)", static_cast<unsigned>(payload.size()));
    return;
  }
  if (this->raw_pending_) {
    ESP_LOGW(TAG, "send_raw: previous raw command still pending");
    return;
  }
  this->raw_cmd_ = command;
  this->raw_payload_ = payload;
  this->raw_pending_ = true;
}

// ---------------------------------------------------------------------------------------------
// Control entry points
// ---------------------------------------------------------------------------------------------

bool FaikinComponent::require_(uint8_t group, const char *what) {
  if (!this->online_) {
    ESP_LOGW(TAG, "Ignoring %s: no connection to the unit", what);
    return false;
  }
  if ((this->state_.known & group) == 0) {
    ESP_LOGW(TAG, "Ignoring %s: not supported by this unit (or not read yet)", what);
    return false;
  }
  return true;
}

bool FaikinComponent::control_power(bool on) {
  if (!this->require_(s21::GROUP_D1, "power"))
    return false;
  this->desired_.power = on;
  this->mark_dirty_(s21::GROUP_D1);
  return true;
}

bool FaikinComponent::control_mode(s21::Mode mode) {
  if (!this->require_(s21::GROUP_D1, "mode"))
    return false;
  this->desired_.mode = mode;
  this->desired_.power = true;
  this->mark_dirty_(s21::GROUP_D1);
  return true;
}

bool FaikinComponent::control_target_temperature(float celsius) {
  if (!this->require_(s21::GROUP_D1, "target temperature"))
    return false;
  this->desired_.target_temp = s21::decode_target_temp(s21::encode_target_temp(celsius));
  this->mark_dirty_(s21::GROUP_D1);
  return true;
}

bool FaikinComponent::control_fan(s21::Fan fan) {
  if (!this->require_(s21::GROUP_D1, "fan"))
    return false;
  this->desired_.fan = fan;
  this->mark_dirty_(s21::GROUP_D1);
  return true;
}

bool FaikinComponent::control_swing(bool horizontal, bool vertical) {
  if (!this->require_(s21::GROUP_D5, "swing"))
    return false;
  this->desired_.swing_h = horizontal;
  this->desired_.swing_v = vertical;
  this->mark_dirty_(s21::GROUP_D5);
  return true;
}

bool FaikinComponent::control_flag(Flag flag, bool value) {
  const bool d7 = (flag == Flag::ECONO);
  if (!this->require_(d7 ? s21::GROUP_D7 : s21::GROUP_D6, "mode switch"))
    return false;
  switch (flag) {
    case Flag::ECONO:
      this->desired_.econo = value;
      break;
    case Flag::POWERFUL:
      this->desired_.powerful = value;
      break;
    case Flag::COMFORT:
      this->desired_.comfort = value;
      break;
    case Flag::QUIET:
      this->desired_.quiet = value;
      break;
    case Flag::STREAMER:
      this->desired_.streamer = value;
      break;
    case Flag::SENSOR:
      this->desired_.sensor_led = value;
      break;
  }
  this->mark_dirty_(d7 ? s21::GROUP_D7 : s21::GROUP_D6);
  return true;
}

bool FaikinComponent::control_demand(int percent) {
  if (!this->require_(s21::GROUP_D7, "demand control"))
    return false;
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;
  this->desired_.demand = percent;
  this->mark_dirty_(s21::GROUP_D7);
  return true;
}

}  // namespace faikin
}  // namespace esphome
