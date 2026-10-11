#include "apszb.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cstring>

namespace esphome {
namespace apszb {

static const char *const TAG = "apszb";

static const uint32_t STARTUP_DELAY_MS = 5000;
static const uint32_t PAIR_STEP_MS = 1500;
static const uint32_t PAIR_RETRY_MS = 120000;
static const uint32_t NORMAL_OPS_RETRY_MS = 300000;
static const uint8_t RESPONSE_RETRIES = 2;
static const uint8_t MAX_TX_ATTEMPTS = 3;
static const uint16_t PERSIST_MAGIC = 0xA5B5;

// wrap-safe "time t has been reached"
static inline bool due(uint32_t now, uint32_t t) { return int32_t(now - t) >= 0; }

static const char *type_name(InverterType t) {
  switch (t) {
    case InverterType::YC600:
      return "YC600";
    case InverterType::QS1:
      return "QS1";
    default:
      return "DS3";
  }
}

// =====================================================================================================================
// APSInverter
// =====================================================================================================================
void APSInverter::set_serial(const std::string &hex) {
  if (!parse_hex_bytes(hex.c_str(), this->serial_, 6))
    ESP_LOGE(TAG, "invalid inverter serial '%s' (12 hex digits expected)", hex.c_str());
  this->serial_hex_ = hex;
}

void APSInverter::setup_() {
  uint32_t hash = fnv1_hash("apszb_" + this->serial_hex_);
  this->pref_ = global_preferences->make_preference<Persist>(hash);
  Persist p{};
  if (this->pref_.load(&p) && p.magic == PERSIST_MAGIC) {
    if (this->configured_short_ == 0)
      this->short_addr_ = p.short_addr;
    if (std::isfinite(p.energy_total_wh) && p.energy_total_wh >= 0)
      this->energy_total_wh_ = p.energy_total_wh;
    this->saved_energy_wh_ = this->energy_total_wh_;
  }
#ifdef USE_TEXT_SENSOR
  if (this->serial_text_ != nullptr)
    this->serial_text_->publish_state(this->serial_hex_);
  if (this->short_address_text_ != nullptr && this->paired())
    this->short_address_text_->publish_state(str_sprintf("0x%04X", this->short_addr_));
#endif
#ifdef USE_SENSOR
  if (this->energy_total_ != nullptr && this->energy_total_wh_ > 0)
    this->energy_total_->publish_state(this->energy_total_wh_);
#endif
  this->publish_status_();
}

void APSInverter::dump_config_() {
  ESP_LOGCONFIG(TAG, "  Inverter %u: %s serial %s, short address %s, calibration %d W", this->index_,
                type_name(this->type_), this->serial_hex_.c_str(),
                this->paired() ? str_sprintf("0x%04X", this->short_addr_).c_str() : "unknown (not paired)",
                this->calibration_);
}

void APSInverter::save_() {
  Persist p{PERSIST_MAGIC, this->short_addr_, this->energy_total_wh_};
  this->pref_.save(&p);
  this->saved_energy_wh_ = this->energy_total_wh_;
  this->last_save_ = millis();
}

void APSInverter::save_energy_if_needed_(uint32_t now) {
  if (now - this->last_save_ < 300000)
    return;
  if (this->energy_total_wh_ - this->saved_energy_wh_ >= 1.0f)
    this->save_();
}

void APSInverter::publish_status_() {
  const char *st = !this->paired() ? "unpaired" : (this->online_state_ ? "online" : "offline");
#ifdef USE_TEXT_SENSOR
  if (this->status_text_ != nullptr && this->last_status_ != st)
    this->status_text_->publish_state(st);
#endif
#ifdef USE_BINARY_SENSOR
  if (this->online_ != nullptr)
    this->online_->publish_state(this->online_state_);
#endif
  this->last_status_ = st;
}

void APSInverter::zero_power_() {
#ifdef USE_SENSOR
  for (auto *s : this->power_)
    if (s != nullptr)
      s->publish_state(0.0f);
  if (this->power_total_ != nullptr)
    this->power_total_->publish_state(0.0f);
#endif
}

void APSInverter::on_paired_(uint16_t short_addr) {
  this->short_addr_ = short_addr;
  this->fail_count_ = 0;
  this->save_();
#ifdef USE_TEXT_SENSOR
  if (this->short_address_text_ != nullptr)
    this->short_address_text_->publish_state(str_sprintf("0x%04X", short_addr));
#endif
  this->publish_status_();
  ESP_LOGI(TAG, "inverter %s paired, short address 0x%04X", this->serial_hex_.c_str(), short_addr);
}

void APSInverter::on_poll_failed_() {
  if (this->fail_count_ < 255)
    this->fail_count_++;
  ESP_LOGD(TAG, "inverter %s: no valid answer (%u in a row)", this->serial_hex_.c_str(), this->fail_count_);
  if (this->online_state_ && this->fail_count_ >= this->parent_->offline_after()) {
    this->online_state_ = false;
    ESP_LOGI(TAG, "inverter %s is offline", this->serial_hex_.c_str());
    this->zero_power_();
    this->publish_status_();
  }
}

void APSInverter::on_poll_(const PollData &d, int8_t rssi, uint8_t lqi) {
  this->fail_count_ = 0;
  const bool was_online = this->online_state_;
  this->online_state_ = true;

  // The inverter does not report an instantaneous power. Like the original firmware, the power of each channel is
  // derived from the increase of its energy counter between two polls.
  const bool first = !this->have_prev_;
  const bool reset = first || d.timestamp < this->prev_ts_;
  const uint32_t dt = reset ? d.timestamp : (d.timestamp - this->prev_ts_);
  const bool power_valid = !first && dt > 0;

  float total_power = 0, total_incr = 0;
  float power[4] = {0};
  for (uint8_t x = 0; x < d.channels; x++) {
    float incr = 0;
    if (!first)
      incr = reset ? d.energy_wh[x] : d.energy_wh[x] - this->prev_energy_[x];
    if (incr < 0)
      incr = 0;
    total_incr += incr;
    if (power_valid) {
      power[x] = incr / float(dt) * 3600.0f;
      total_power += power[x];
    }
    this->prev_energy_[x] = d.energy_wh[x];
  }
  this->prev_ts_ = d.timestamp;
  this->have_prev_ = true;
  this->energy_total_wh_ += total_incr;

  ESP_LOGD(TAG, "inverter %s: %.1f V %.2f Hz %.1f C  PV1 %.1f V %.2f A  PV2 %.1f V %.2f A  P=%.1f W  E=%.2f Wh",
           this->serial_hex_.c_str(), d.ac_voltage, d.frequency, d.temperature, d.dc_voltage[0], d.dc_current[0],
           d.dc_voltage[1], d.dc_current[1], total_power, this->energy_total_wh_);

#ifdef USE_SENSOR
  if (this->ac_voltage_ != nullptr)
    this->ac_voltage_->publish_state(d.ac_voltage);
  if (this->frequency_ != nullptr)
    this->frequency_->publish_state(d.frequency);
  if (this->temperature_ != nullptr)
    this->temperature_->publish_state(d.temperature);
  if (this->link_quality_ != nullptr)
    this->link_quality_->publish_state(float(lqi) * 100.0f / 255.0f);
  if (this->rssi_ != nullptr && rssi != RSSI_UNKNOWN)
    this->rssi_->publish_state(rssi);
  if (this->uptime_ != nullptr)
    this->uptime_->publish_state(float(d.timestamp));
  for (uint8_t x = 0; x < d.channels; x++) {
    if (this->dc_voltage_[x] != nullptr)
      this->dc_voltage_[x]->publish_state(d.dc_voltage[x]);
    if (this->dc_current_[x] != nullptr)
      this->dc_current_[x]->publish_state(d.dc_current[x]);
    if (this->energy_[x] != nullptr)
      this->energy_[x]->publish_state(d.energy_wh[x]);
    if (power_valid && this->power_[x] != nullptr)
      this->power_[x]->publish_state(power[x]);
  }
  if (power_valid && this->power_total_ != nullptr)
    this->power_total_->publish_state(total_power);
  if (this->energy_total_ != nullptr)
    this->energy_total_->publish_state(this->energy_total_wh_);
#endif
  if (!was_online)
    ESP_LOGI(TAG, "inverter %s is online", this->serial_hex_.c_str());
  this->publish_status_();
}

void APSInverter::on_max_power_(float watts_with_calibration) {
  float w = watts_with_calibration - float(this->calibration_);
  ESP_LOGD(TAG, "inverter %s: programmed max power %.0f W (raw %.1f)", this->serial_hex_.c_str(), w,
           watts_with_calibration);
#ifdef USE_NUMBER
  if (this->max_power_number_ != nullptr)
    this->max_power_number_->publish_state(roundf(w));
#endif
}

void APSInverter::request_pair() { this->parent_->enqueue_op(OpKind::PAIR, this->index_); }
void APSInverter::request_refresh() { this->parent_->enqueue_op(OpKind::POLL, this->index_); }
void APSInverter::request_query() { this->parent_->enqueue_op(OpKind::QUERY, this->index_); }
void APSInverter::request_reboot() { this->parent_->enqueue_op(OpKind::REBOOT, this->index_); }
void APSInverter::request_max_power(float watts) {
  this->parent_->enqueue_op(OpKind::THROTTLE, this->index_, int32_t(roundf(watts)));
}

// =====================================================================================================================
// Hub
// =====================================================================================================================
void APSZBComponent::setup() {
  uint32_t now = millis();
  this->start_time_ = now;
  uint8_t id[6];
  if (!parse_hex_bytes(this->ecu_id_hex_.c_str(), id, 6)) {
    ESP_LOGE(TAG, "invalid ecu_id '%s'", this->ecu_id_hex_.c_str());
    this->mark_failed();
    return;
  }
  this->cfg_.set_ecu_id(id);
  this->cfg_.ecu_reverse(this->ecu_rev_);

#ifdef USE_APSZB_NATIVE
  if (!this->radio_.begin(this->cfg_, this->tx_power_, this->promiscuous_)) {
    ESP_LOGE(TAG, "802.15.4 radio initialisation failed");
    this->mark_failed();
    return;
  }
#endif
#ifdef USE_APSZB_ZNP
  if (this->uart_ == nullptr) {
    ESP_LOGE(TAG, "no UART configured for the ZNP modem");
    this->mark_failed();
    return;
  }
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }
  this->znp_port_.uart = this->uart_;
  this->znp_port_.reset = this->reset_pin_;
  this->znp_.set_port(&this->znp_port_);
  this->znp_.set_log_frames(this->log_frames_);
  this->znp_.set_logger([](int level, const char *msg) {
    switch (level) {
      case 0:
        ESP_LOGE(TAG, "%s", msg);
        break;
      case 1:
        ESP_LOGW(TAG, "%s", msg);
        break;
      case 2:
        ESP_LOGI(TAG, "%s", msg);
        break;
      case 3:
        ESP_LOGD(TAG, "%s", msg);
        break;
      default:
        ESP_LOGV(TAG, "%s", msg);
        break;
    }
  });
  this->znp_.begin(this->cfg_);
#endif
  for (auto *inv : this->inverters_)
    inv->setup_();

  this->next_poll_ = now + STARTUP_DELAY_MS + 3000;
  this->next_link_status_ = now + STARTUP_DELAY_MS;
  this->next_rreq_ = now + STARTUP_DELAY_MS + 1000;
  this->next_normal_ops_ = now + NORMAL_OPS_RETRY_MS;
  this->next_pair_try_ = now + STARTUP_DELAY_MS + 6000;
}

void APSZBComponent::dump_config() {
#ifdef USE_APSZB_NATIVE
  ESP_LOGCONFIG(TAG, "APsystems ZigBee (native 802.15.4 radio):");
#else
  ESP_LOGCONFIG(TAG, "APsystems ZigBee (CC2530 ZNP modem on UART):");
  LOG_PIN("  Reset pin: ", this->reset_pin_);
#endif
  ESP_LOGCONFIG(TAG, "  ECU id: %s  (PAN 0x%04X)", this->ecu_id_hex_.c_str(), this->cfg_.pan_id);
#ifdef USE_APSZB_NATIVE
  ESP_LOGCONFIG(TAG, "  Channel: %u, TX power: %d dBm", this->cfg_.channel, this->tx_power_);
#else
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->cfg_.channel);
#endif
  ESP_LOGCONFIG(TAG, "  Poll interval: %u ms, answer timeout: %u ms", (unsigned) this->poll_interval_,
                (unsigned) this->response_timeout_);
#ifdef USE_APSZB_NATIVE
  ESP_LOGCONFIG(TAG, "  NWK protocol version: %u, MAC ack: %s, discover route: %s", this->cfg_.nwk_protocol_version,
                YESNO(this->cfg_.mac_ack), YESNO(this->cfg_.discover_route));
  ESP_LOGCONFIG(TAG, "  Link status every %u ms, many-to-one route request every %u ms",
                (unsigned) this->link_status_interval_, (unsigned) this->route_request_interval_);
  ESP_LOGCONFIG(TAG, "  Auto pair: %s, promiscuous: %s, frame log: %s", YESNO(this->auto_pair_),
                YESNO(this->promiscuous_), YESNO(this->log_frames_));
#else
  ESP_LOGCONFIG(TAG, "  Auto pair: %s, frame log: %s", YESNO(this->auto_pair_), YESNO(this->log_frames_));
#endif
  for (auto *inv : this->inverters_)
    inv->dump_config_();
}

void APSZBComponent::loop() {
  const uint32_t now = millis();
#ifdef USE_APSZB_NATIVE
  RadioRx rx;
  for (int guard = 0; guard < 8 && this->radio_.pop(rx); guard++)
    this->handle_rx_(rx);
#endif
#ifdef USE_APSZB_ZNP
  this->znp_.loop(now);
  ZnpRx zrx;
  for (int guard = 0; guard < 8 && this->znp_.pop(zrx); guard++)
    this->handle_znp_rx_(zrx);
#endif
  this->process_tx_(now);
  this->run_op_(now);
  this->housekeeping_(now);
}

// ---------------------------------------------------------------------------------------------------------------------
// periodic work
// ---------------------------------------------------------------------------------------------------------------------
void APSZBComponent::housekeeping_(uint32_t now) {
  if (due(now, this->next_poll_)) {
    this->next_poll_ = now + this->poll_interval_;
    bool any_offline = false;
    for (auto *inv : this->inverters_)
      if (inv->paired() && !inv->online())
        any_offline = true;
    // an inverter that was switched off (night) needs to hear the ECU "normal operation" announcement again
    if (!this->normal_ops_sent_ || (any_offline && due(now, this->next_normal_ops_))) {
      this->enqueue_op(OpKind::NORMAL_OPS, 0);
      this->normal_ops_sent_ = true;
      this->next_normal_ops_ = now + NORMAL_OPS_RETRY_MS;
    }
    for (auto *inv : this->inverters_)
      if (inv->paired())
        this->enqueue_op(OpKind::POLL, inv->index());
  }

  if (this->auto_pair_ && due(now, this->next_pair_try_)) {
    this->next_pair_try_ = now + PAIR_RETRY_MS;
    for (auto *inv : this->inverters_)
      if (!inv->paired())
        this->enqueue_op(OpKind::PAIR, inv->index());
  }

#ifdef USE_APSZB_NATIVE
  // Behave like a ZigBee router/concentrator towards the inverters (they are ZigBee routers themselves).
  // With the CC2530 the Z-Stack firmware does this on its own.
  if (this->link_status_interval_ != 0 && due(now, this->next_link_status_)) {
    this->next_link_status_ = now + this->link_status_interval_;
    uint16_t nb[16];
    size_t n = 0;
    for (auto *inv : this->inverters_)
      if (inv->paired() && n < 16)
        nb[n++] = inv->short_address();
    TxFrame f;
    if (this->factory_.build_link_status(f, nb, n))
      this->queue_tx_(f);
  }
  if (this->route_request_interval_ != 0 && due(now, this->next_rreq_)) {
    this->next_rreq_ = now + this->route_request_interval_;
    TxFrame f;
    if (this->factory_.build_many_to_one_rreq(f))
      this->queue_tx_(f);
  }
#endif
  for (auto *inv : this->inverters_)
    inv->save_energy_if_needed_(now);
}

bool APSZBComponent::op_queued_(OpKind kind, uint8_t inv) const {
  if (this->op_active_ && this->cur_.kind == kind && this->cur_.inv == inv)
    return true;
  for (const auto &op : this->ops_)
    if (op.kind == kind && op.inv == inv)
      return true;
  return false;
}

void APSZBComponent::enqueue_op(OpKind kind, uint8_t inv, int32_t arg) {
  if (inv >= this->inverters_.size() && kind != OpKind::NORMAL_OPS)
    return;
  if (kind != OpKind::THROTTLE && this->op_queued_(kind, inv))
    return;
  if (this->ops_.size() > 24) {
    ESP_LOGW(TAG, "operation queue full, dropping request");
    return;
  }
  this->ops_.push_back(Op{kind, inv, arg});
}

// ---------------------------------------------------------------------------------------------------------------------
// TX engine (serialises all transmissions, retries on missing MAC ack / busy channel)
// ---------------------------------------------------------------------------------------------------------------------
uint32_t APSZBComponent::queue_tx_(const TxFrame &f) {
  if (this->txq_.size() > 16)
    this->txq_.pop_front();
  TxJob job;
  job.frame = f;
  job.id = ++this->tx_id_counter_;
  this->txq_.push_back(job);
  return job.id;
}

bool APSZBComponent::radio_ready_() const {
#ifdef USE_APSZB_ZNP
  return this->znp_.ready();
#else
  return true;
#endif
}

bool APSZBComponent::radio_transmit_(const TxFrame &f) {
#ifdef USE_APSZB_ZNP
  return this->znp_.transmit(f);
#else
  return this->radio_.transmit(f);
#endif
}

int APSZBComponent::radio_tx_result_() const {
#ifdef USE_APSZB_ZNP
  return this->znp_.tx_result();
#else
  return this->radio_.tx_result();
#endif
}

const char *APSZBComponent::radio_tx_error_(int code) const {
#ifdef USE_APSZB_ZNP
  return ZnpRadio::tx_error_name(code);
#else
  return ApsRadio::tx_error_name(code);
#endif
}

void APSZBComponent::process_tx_(uint32_t now) {
  if (this->tx_active_) {
    int r = this->radio_tx_result_();
    if (r == -1) {
      if (now - this->tx_started_ < 300)
        return;
      r = 98;  // the driver never reported a result
    }
    this->tx_active_ = false;
    if (r == 0) {
      this->tx_done_id_ = this->cur_tx_.id;
      this->last_tx_ok_ = true;
    } else if (this->cur_tx_.attempts < MAX_TX_ATTEMPTS) {
      ESP_LOGV(TAG, "TX attempt %u failed (%s), retrying", this->cur_tx_.attempts, this->radio_tx_error_(r));
      this->tx_not_before_ = now + 4 + (random_uint32() % 12);
      this->txq_.push_front(this->cur_tx_);
    } else {
      ESP_LOGD(TAG, "TX failed after %u attempts: %s", this->cur_tx_.attempts, this->radio_tx_error_(r));
      this->tx_done_id_ = this->cur_tx_.id;
      this->last_tx_ok_ = false;
    }
  }
  if (!this->tx_active_ && !this->txq_.empty() && due(now, this->tx_not_before_) && this->radio_ready_()) {
    this->cur_tx_ = this->txq_.front();
    this->txq_.pop_front();
    this->cur_tx_.attempts++;
#ifdef USE_APSZB_NATIVE
    if (this->log_frames_)
      ESP_LOGI(TAG, "TX %u bytes: %s", (unsigned) this->cur_tx_.frame.mac_len,
               format_hex_pretty(this->cur_tx_.frame.buf + 1, this->cur_tx_.frame.mac_len).c_str());
#endif
    if (this->radio_transmit_(this->cur_tx_.frame)) {
      this->tx_active_ = true;
      this->tx_started_ = now;
    } else {
      this->tx_done_id_ = this->cur_tx_.id;
      this->last_tx_ok_ = false;
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// RX
// ---------------------------------------------------------------------------------------------------------------------
#ifdef USE_APSZB_ZNP
// The CC2530 already did MAC / NWK / APS (including fragment reassembly and APS acknowledgements): the message arrives
// ready to use.
void APSZBComponent::handle_znp_rx_(const ZnpRx &m) {
  if (m.src == ADDR_COORDINATOR)
    return;
  RxFrame f;
  f.mac_ok = f.nwk_ok = f.aps_ok = true;
  f.mac_type = 1;
  f.nwk_src = m.src;
  f.nwk_dst = ADDR_COORDINATOR;
  f.cluster = m.cluster;
  f.profile = APS_PROFILE_ID;
  f.payload = m.data;
  f.payload_len = m.len;
  this->handle_asdu_(f, m.data, m.len, RSSI_UNKNOWN, m.lqi);
}
#endif

#ifdef USE_APSZB_NATIVE
void APSZBComponent::handle_rx_(const RadioRx &rx) {
  RxFrame f;
  if (!parse_frame(rx.data, rx.len, f))
    return;
  if (this->log_frames_)
    ESP_LOGI(TAG, "RX %u bytes rssi=%d lqi=%u: %s", rx.len, rx.rssi, rx.lqi, format_hex_pretty(rx.data, rx.len).c_str());
  if (!f.mac_ok || f.mac_type != 1)
    return;
  if (f.nwk_ok && f.nwk_secured) {
    if (!this->warned_secured_) {
      ESP_LOGW(TAG, "NWK-secured frame from 0x%04X ignored: this component only supports the unsecured APS network",
               f.nwk_src);
      this->warned_secured_ = true;
    }
    return;
  }
  if (!f.nwk_ok)
    return;
  if (f.nwk_type == 1) {
    ESP_LOGV(TAG, "NWK command 0x%02X from 0x%04X", f.nwk_cmd, f.nwk_src);
    return;
  }
  if (!f.aps_ok || f.aps_type != 0 || f.payload == nullptr)
    return;
  // ignore our own broadcasts re-broadcast by the routers, and frames not meant for us (promiscuous mode)
  if (f.nwk_src == ADDR_COORDINATOR)
    return;
  if (f.nwk_dst != ADDR_COORDINATOR && f.nwk_dst < 0xFFF8)
    return;
  if (f.aps_ack_request && f.aps_delivery == 0 && f.nwk_dst == ADDR_COORDINATOR) {
    TxFrame ack;
    if (this->factory_.build_aps_ack(ack, f))
      this->queue_tx_(ack);
  }
  const uint8_t *asdu = nullptr;
  size_t n = 0;
  if (!this->reasm_.feed(f, millis(), &asdu, &n))
    return;
  this->handle_asdu_(f, asdu, n, rx.rssi, rx.lqi);
}
#endif  // USE_APSZB_NATIVE

int APSZBComponent::find_inverter_(const uint8_t *asdu, size_t n, bool anywhere, size_t *offset) const {
  for (size_t i = 0; i < this->inverters_.size(); i++) {
    const uint8_t *serial = this->inverters_[i]->serial();
    if (asdu_has_serial(asdu, n, serial)) {
      *offset = 0;
      return int(i);
    }
    if (anywhere) {
      for (size_t k = 1; k + 6 <= n; k++) {
        if (memcmp(asdu + k, serial, 6) == 0) {
          *offset = k;
          return int(i);
        }
      }
    }
  }
  return -1;
}

bool APSZBComponent::plausible_(const PollData &d) const {
  return d.ac_voltage >= 0 && d.ac_voltage < 400 && d.frequency > 40 && d.frequency < 70 && d.temperature > -60 &&
         d.temperature < 150;
}

void APSZBComponent::handle_asdu_(const RxFrame &f, const uint8_t *asdu, size_t n, int8_t rssi, uint8_t lqi) {
  const bool pairing = this->op_active_ && this->cur_.kind == OpKind::PAIR;
  size_t off = 0;
  int idx = this->find_inverter_(asdu, n, pairing, &off);
  if (idx < 0) {
    ESP_LOGV(TAG, "payload from 0x%04X (cluster 0x%04X) matches no configured inverter: %s", f.nwk_src, f.cluster,
             format_hex_pretty(asdu, n).c_str());
    return;
  }
  APSInverter *inv = this->inverters_[idx];

  if (pairing) {
    if (idx == this->cur_.inv && f.nwk_src != ADDR_COORDINATOR && f.nwk_src < 0xFFF8) {
      this->pair_addr_ = f.nwk_src;
      ESP_LOGI(TAG, "pairing: inverter %s answered from 0x%04X (cluster 0x%04X)", inv->serial_hex().c_str(), f.nwk_src,
               f.cluster);
      if (off + 8 <= n) {
        uint16_t embedded = uint16_t(asdu[off + 6]) | (uint16_t(asdu[off + 7]) << 8);
        if (embedded != f.nwk_src)
          ESP_LOGD(TAG, "pairing: address in the payload (0x%04X) differs from the NWK source (0x%04X)", embedded,
                   f.nwk_src);
      }
    }
    return;
  }

  // The answer window opens as soon as the request that precedes the AWAIT step has been handed to the radio: on a
  // busy main loop the inverter can answer before the state machine has actually reached the AWAIT step.
  bool awaiting = false;
  if (this->op_active_ && idx == this->cur_.inv && this->step_idx_ < this->steps_.size()) {
    const Step::Type t = this->steps_[this->step_idx_].type;
    awaiting = (t == Step::AWAIT) || (t == Step::SEND && this->step_idx_ + 1 < this->steps_.size() &&
                                      this->steps_[this->step_idx_ + 1].type == Step::AWAIT);
  }
  if (!awaiting) {
    ESP_LOGV(TAG, "unsolicited answer from inverter %s", inv->serial_hex().c_str());
    return;
  }
  if (inv->paired() && f.nwk_src != inv->short_address())
    ESP_LOGW(TAG, "answer from 0x%04X but inverter %s is paired as 0x%04X (press the pair button to refresh)",
             f.nwk_src, inv->serial_hex().c_str(), inv->short_address());

  if (this->cur_.kind == OpKind::POLL) {
    PollData d;
    if (!decode_poll(asdu, n, inv->type(), d)) {
      ESP_LOGD(TAG, "inverter %s: poll answer too short or malformed (%u bytes): %s", inv->serial_hex().c_str(),
               (unsigned) n, format_hex_pretty(asdu, n).c_str());
      return;
    }
    if (!this->plausible_(d)) {
      ESP_LOGD(TAG, "inverter %s: implausible values (%.1f V, %.2f Hz, %.1f C), answer ignored",
               inv->serial_hex().c_str(), d.ac_voltage, d.frequency, d.temperature);
      return;
    }
    inv->on_poll_(d, rssi, lqi);
    this->got_response_ = true;
    this->op_ok_ = true;
  } else if (this->cur_.kind == OpKind::QUERY || this->cur_.kind == OpKind::THROTTLE) {
    float w;
    if (!decode_max_power(asdu, n, inv->type(), &w)) {
      ESP_LOGD(TAG, "inverter %s: max power not found in the answer: %s", inv->serial_hex().c_str(),
               format_hex_pretty(asdu, n).c_str());
      return;
    }
    inv->on_max_power_(w);
    this->got_response_ = true;
    this->op_ok_ = true;
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// operation engine
// ---------------------------------------------------------------------------------------------------------------------
void APSZBComponent::add_unicast_(uint16_t dst, uint16_t cluster, const uint8_t *asdu, size_t n) {
  Step s;
  s.type = Step::SEND;
  if (this->factory_.build_unicast(s.frame, dst, cluster, asdu, n))
    this->steps_.push_back(s);
}

void APSZBComponent::add_broadcast_(uint16_t pan, uint16_t cluster, const uint8_t *asdu, size_t n) {
  Step s;
  s.type = Step::SEND;
  if (this->factory_.build_broadcast(s.frame, pan, cluster, asdu, n))
    this->steps_.push_back(s);
}

void APSZBComponent::add_delay_(uint32_t ms) {
  Step s;
  s.type = Step::DELAY;
  s.ms = ms;
  this->steps_.push_back(s);
}

void APSZBComponent::add_await_(uint32_t ms) {
  Step s;
  s.type = Step::AWAIT;
  s.ms = ms;
  this->steps_.push_back(s);
}

void APSZBComponent::build_steps_() {
  this->steps_.clear();
  this->step_idx_ = 0;
  this->send_issued_ = false;
  this->got_response_ = false;
  if (this->cur_.inv >= this->inverters_.size() && this->cur_.kind != OpKind::NORMAL_OPS)
    return;
  APSInverter *inv = this->cur_.inv < this->inverters_.size() ? this->inverters_[this->cur_.inv] : nullptr;
  uint8_t asdu[MAX_ASDU];
  size_t n;
  switch (this->cur_.kind) {
    case OpKind::POLL:
    case OpKind::QUERY:
      if (!inv->paired())
        return;
      n = this->cur_.kind == OpKind::POLL ? asdu_poll(asdu, this->ecu_rev_) : asdu_query(asdu, this->ecu_rev_);
      this->add_unicast_(inv->short_address(), CLUSTER_ECU_TO_INV, asdu, n);
      this->add_await_(this->response_timeout_);
      break;
    case OpKind::THROTTLE:
      if (!inv->paired())
        return;
      ESP_LOGI(TAG, "inverter %s: setting max power to %d W (calibration %+d W)", inv->serial_hex().c_str(),
               (int) this->cur_.arg, inv->calibration());
      n = asdu_throttle(asdu, this->ecu_rev_, inv->type(), this->cur_.arg + inv->calibration());
      this->add_unicast_(inv->short_address(), CLUSTER_ECU_TO_INV, asdu, n);
      this->add_delay_(800);
      n = asdu_query_flush(asdu, this->ecu_rev_);
      this->add_unicast_(inv->short_address(), CLUSTER_ECU_TO_INV, asdu, n);
      this->add_delay_(800);
      n = asdu_query(asdu, this->ecu_rev_);  // read back the programmed value
      this->add_unicast_(inv->short_address(), CLUSTER_ECU_TO_INV, asdu, n);
      this->add_await_(this->response_timeout_);
      break;
    case OpKind::REBOOT:
      if (!inv->paired())
        return;
      ESP_LOGI(TAG, "inverter %s: sending reboot command", inv->serial_hex().c_str());
      n = asdu_reboot(asdu, this->ecu_rev_);
      this->add_unicast_(inv->short_address(), CLUSTER_ECU_TO_INV, asdu, n);
      this->add_delay_(1000);
      break;
    case OpKind::NORMAL_OPS:
      ESP_LOGD(TAG, "sending ECU 'normal operation' broadcast");
      n = asdu_normal_ops(asdu, this->ecu_rev_);
      this->add_broadcast_(this->cfg_.pan_id, CLUSTER_ECU_TO_INV, asdu, n);
      this->add_delay_(250);
      this->add_broadcast_(this->cfg_.pan_id, CLUSTER_ECU_TO_INV, asdu, n);
      this->add_delay_(500);
      break;
    case OpKind::PAIR:
      ESP_LOGI(TAG, "pairing inverter %s (the inverter must be powered by the PV panels)", inv->serial_hex().c_str());
      this->pair_addr_ = 0;
      for (int step = 0; step < 4; step++) {
        uint16_t cluster = 0;
        n = asdu_pair(asdu, step, this->cfg_, inv->serial(), &cluster);
        this->add_broadcast_(ADDR_BROADCAST, cluster, asdu, n);  // destination PAN 0xFFFF: reaches unassociated devices
        this->add_delay_(PAIR_STEP_MS);
      }
      break;
  }
}

void APSZBComponent::start_op_(const Op &op) {
  this->cur_ = op;
  this->op_active_ = true;
  this->attempt_ = 0;
  this->op_ok_ = !(op.kind == OpKind::POLL || op.kind == OpKind::QUERY || op.kind == OpKind::THROTTLE);
  this->build_steps_();
  this->step_started_ = millis();
  if (this->steps_.empty())
    this->finish_op_(false);
}

void APSZBComponent::finish_op_(bool ok) {
  APSInverter *inv = this->cur_.inv < this->inverters_.size() ? this->inverters_[this->cur_.inv] : nullptr;
  switch (this->cur_.kind) {
    case OpKind::POLL:
      if (!ok && inv != nullptr && inv->paired())
        inv->on_poll_failed_();
      break;
    case OpKind::QUERY:
    case OpKind::THROTTLE:
      if (!ok && inv != nullptr)
        ESP_LOGW(TAG, "inverter %s: no answer to the max power query", inv->serial_hex().c_str());
      break;
    case OpKind::PAIR:
      if (inv != nullptr) {
        if (this->pair_addr_ != 0) {
          inv->on_paired_(this->pair_addr_);
          this->enqueue_op(OpKind::NORMAL_OPS, 0);
          this->enqueue_op(OpKind::POLL, this->cur_.inv);
        } else {
          ESP_LOGW(TAG, "pairing of %s failed: no answer (is the inverter awake and in range?)",
                   inv->serial_hex().c_str());
        }
      }
      break;
    default:
      break;
  }
  this->op_active_ = false;
  this->steps_.clear();
}

void APSZBComponent::run_op_(uint32_t now) {
  if (!this->op_active_) {
    if (this->ops_.empty() || !this->radio_ready_())
      return;
    Op op = this->ops_.front();
    this->ops_.pop_front();
    this->start_op_(op);
    return;
  }
  if (this->step_idx_ >= this->steps_.size()) {
    this->finish_op_(this->op_ok_);
    return;
  }
  Step &s = this->steps_[this->step_idx_];
  switch (s.type) {
    case Step::SEND:
      if (!this->send_issued_) {
        this->send_job_id_ = this->queue_tx_(s.frame);
        this->send_issued_ = true;
      } else if (this->tx_done_id_ >= this->send_job_id_) {
        this->send_issued_ = false;
        if (!this->last_tx_ok_ && s.frame.ack_request) {
          ESP_LOGD(TAG, "no MAC acknowledgement from the inverter");
          this->finish_op_(false);
          return;
        }
        this->step_idx_++;
        this->step_started_ = now;
      }
      break;
    case Step::DELAY:
      if (now - this->step_started_ >= s.ms) {
        this->step_idx_++;
        this->step_started_ = now;
      }
      break;
    case Step::AWAIT:
      if (this->got_response_) {
        this->step_idx_++;
        this->step_started_ = now;
      } else if (now - this->step_started_ >= s.ms) {
        if (this->attempt_ < RESPONSE_RETRIES) {
          this->attempt_++;
          ESP_LOGV(TAG, "no answer, retry %u", this->attempt_);
          this->build_steps_();
          this->step_started_ = now;
        } else {
          this->finish_op_(false);
        }
      }
      break;
  }
}

}  // namespace apszb
}  // namespace esphome
