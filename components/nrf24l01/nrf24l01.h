#pragma once

#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace nrf24l01 {

// ---------------------------------------------------------------------------
// Registers and commands ported from nRF24L01.h (Stefan Engelke / TMRh20
// nRF24/RF24 -- nRF24L01+ datasheet register map, stable for 15+ years,
// cross-checked against several independent implementations).
// ---------------------------------------------------------------------------
static const uint8_t REG_CONFIG = 0x00;
static const uint8_t REG_EN_AA = 0x01;
static const uint8_t REG_EN_RXADDR = 0x02;
static const uint8_t REG_SETUP_AW = 0x03;
static const uint8_t REG_SETUP_RETR = 0x04;
static const uint8_t REG_RF_CH = 0x05;
static const uint8_t REG_RF_SETUP = 0x06;
static const uint8_t REG_STATUS = 0x07;
static const uint8_t REG_OBSERVE_TX = 0x08;
static const uint8_t REG_RPD = 0x09;
static const uint8_t REG_RX_ADDR_P0 = 0x0A;
static const uint8_t REG_RX_ADDR_P1 = 0x0B;
static const uint8_t REG_RX_ADDR_P2 = 0x0C;
static const uint8_t REG_RX_ADDR_P3 = 0x0D;
static const uint8_t REG_RX_ADDR_P4 = 0x0E;
static const uint8_t REG_RX_ADDR_P5 = 0x0F;
static const uint8_t REG_TX_ADDR = 0x10;
static const uint8_t REG_RX_PW_P0 = 0x11;
static const uint8_t REG_FIFO_STATUS = 0x17;
static const uint8_t REG_DYNPD = 0x1C;
static const uint8_t REG_FEATURE = 0x1D;

// CONFIG
static const uint8_t MASK_RX_DR = 1 << 6;
static const uint8_t MASK_TX_DS = 1 << 5;
static const uint8_t MASK_MAX_RT = 1 << 4;
static const uint8_t MASK_EN_CRC = 1 << 3;
static const uint8_t MASK_CRCO = 1 << 2;
static const uint8_t MASK_PWR_UP = 1 << 1;
static const uint8_t MASK_PRIM_RX = 1 << 0;

// STATUS
static const uint8_t STATUS_RX_DR = 1 << 6;
static const uint8_t STATUS_TX_DS = 1 << 5;
static const uint8_t STATUS_MAX_RT = 1 << 4;
static const uint8_t STATUS_RX_P_NO_MASK = 0x0E;
static const uint8_t STATUS_TX_FULL = 1 << 0;

// FIFO_STATUS
static const uint8_t FIFO_TX_FULL = 1 << 5;
static const uint8_t FIFO_TX_EMPTY = 1 << 4;
static const uint8_t FIFO_RX_FULL = 1 << 1;
static const uint8_t FIFO_RX_EMPTY = 1 << 0;

// FEATURE
static const uint8_t FEATURE_EN_DPL = 1 << 2;
static const uint8_t FEATURE_EN_ACK_PAY = 1 << 1;
static const uint8_t FEATURE_EN_DYN_ACK = 1 << 0;

// Commandes SPI
static const uint8_t CMD_R_REGISTER = 0x00;
static const uint8_t CMD_W_REGISTER = 0x20;
static const uint8_t CMD_REGISTER_MASK = 0x1F;
static const uint8_t CMD_ACTIVATE = 0x50;
static const uint8_t CMD_R_RX_PL_WID = 0x60;
static const uint8_t CMD_R_RX_PAYLOAD = 0x61;
static const uint8_t CMD_W_TX_PAYLOAD = 0xA0;
static const uint8_t CMD_W_ACK_PAYLOAD = 0xA8;
static const uint8_t CMD_FLUSH_TX = 0xE1;
static const uint8_t CMD_FLUSH_RX = 0xE2;
static const uint8_t CMD_REUSE_TX_PL = 0xE3;
static const uint8_t CMD_NOP = 0xFF;

enum PALevel : uint8_t { PA_MIN = 0, PA_LOW = 1, PA_HIGH = 2, PA_MAX = 3 };
enum DataRate : uint8_t { RATE_1MBPS = 0, RATE_2MBPS = 1, RATE_250KBPS = 2 };
enum CRCLength : uint8_t { CRC_DISABLED = 0, CRC_8BIT = 1, CRC_16BIT = 2 };

class NRF24Component : public Component,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_4MHZ> {
 public:
  void set_ce_pin(GPIOPin *pin) { this->ce_pin_ = pin; }
  void set_irq_pin(InternalGPIOPin *pin) { this->irq_pin_ = pin; }

  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_pa_level(uint8_t level) { this->pa_level_ = static_cast<PALevel>(level); }
  /// Applies a PA level immediately (2-3 register writes, no reset) --
  /// unlike set_pa_level() above, used at config time, callable
  /// at any time after setup(). Republishes the state on the associated select, if any.
  void apply_pa_level_runtime(uint8_t level) {
    this->pa_level_ = static_cast<PALevel>(level);
    this->apply_pa_level_();
#ifdef USE_SELECT
    if (this->pa_level_select_ != nullptr) {
      static const char *const kLevels[4] = {"min", "low", "high", "max"};
      this->pa_level_select_->publish_state(kLevels[level > 3 ? 3 : level]);
    }
#endif
  }
#ifdef USE_SELECT
  void set_pa_level_select(select::Select *s) { this->pa_level_select_ = s; }
#endif
  void set_data_rate(uint8_t rate) { this->data_rate_ = static_cast<DataRate>(rate); }
  void set_crc_length(uint8_t len) { this->crc_length_ = static_cast<CRCLength>(len); }
  void set_address_width(uint8_t width) { this->address_width_ = width; }
  void set_auto_ack(bool enable) { this->auto_ack_ = enable; }
  void set_retry_delay(uint8_t d) { this->retry_delay_ = d; }
  void set_retry_count(uint8_t c) { this->retry_count_ = c; }
  void set_payload_size(uint8_t size) { this->payload_size_ = size; }
  void set_dynamic_payloads(bool enable) { this->dynamic_payloads_ = enable; }
  void set_tx_address(const std::vector<uint8_t> &addr) {
    for (uint8_t i = 0; i < 5 && i < addr.size(); i++) this->tx_address_[i] = addr[i];
  }
  void set_rx_address(const std::vector<uint8_t> &addr) {
    for (uint8_t i = 0; i < 5 && i < addr.size(); i++) this->rx_address_[i] = addr[i];
  }

  /// When true: setup() only does SPI/CE init + chip detection. No "generic"
  /// register config (data rate/CRC/pipes/listening), and loop() does nothing --
  /// the calling component (e.g. hm) drives everything via the low-level API below.
  /// Required for the Hoymiles NRF protocol, which does continuous channel hopping
  /// and imposes its own data rate/CRC/address width/retries, incompatible
  /// with generic mode's static config.
  void set_external_mode(bool external) { this->external_mode_ = external; }
  bool get_external_mode() const { return this->external_mode_; }

  // ---------------------------------------------------------------------------
  // Arbitration for several "external_mode" components sharing the same chip
  // (e.g. several hm: on a single nrf24l01:). Same API as cmt2300a. owner is
  // an opaque identifier (typically the caller's 'this').
  // ---------------------------------------------------------------------------
  bool try_lock_external(const void *owner) {
    if (this->external_lock_owner_ == nullptr) {
      this->external_lock_owner_ = owner;
      this->duty_lock_start_ms_ = millis();
      return true;
    }
    if (this->external_lock_owner_ == owner) return true;
    return false;
  }
  void unlock_external(const void *owner) {
    if (this->external_lock_owner_ == owner) {
      this->duty_busy_accum_ms_ += millis() - this->duty_lock_start_ms_;
      this->external_lock_owner_ = nullptr;
    }
  }
  bool is_owned_by_other(const void *owner) const {
    return this->external_lock_owner_ != nullptr && this->external_lock_owner_ != owner;
  }

  /// Radio duty cycle (% of time the lock was held) since the last call, then
  /// resets the measurement window -- same implementation as cmt2300a.
  float get_duty_cycle_percent_and_reset() {
    uint32_t now = millis();
    uint32_t window_ms = now - this->duty_window_start_ms_;
    uint32_t busy_ms = this->duty_busy_accum_ms_;
    if (this->external_lock_owner_ != nullptr) {
      busy_ms += now - this->duty_lock_start_ms_;
      this->duty_lock_start_ms_ = now;
    }
    this->duty_busy_accum_ms_ = 0;
    this->duty_window_start_ms_ = now;
    if (window_ms == 0) return 0.0f;
    return (static_cast<float>(busy_ms) / static_cast<float>(window_ms)) * 100.0f;
  }

  // ---------------------------------------------------------------------------
  // Tracking the number of "hm" components attached to this nrf24l01: that are
  // currently reachable. Same mechanism as cmt2300a/hms: each hm: registers
  // itself once at startup, then reports its reachable state on every change.
  // ---------------------------------------------------------------------------
#ifdef USE_SENSOR
  void set_hm_count_sensor(sensor::Sensor *s) { this->hm_count_sensor_ = s; }
#endif

  void register_reachable_consumer(const void *owner) {
    for (uint8_t i = 0; i < this->reachable_consumer_count_; i++) {
      if (this->reachable_consumers_[i].owner == owner) return;
    }
    if (this->reachable_consumer_count_ < MAX_REACHABLE_CONSUMERS) {
      this->reachable_consumers_[this->reachable_consumer_count_].owner = owner;
      this->reachable_consumers_[this->reachable_consumer_count_].reachable = false;
      this->reachable_consumer_count_++;
    }
  }

  void report_reachable(const void *owner, bool reachable) {
    for (uint8_t i = 0; i < this->reachable_consumer_count_; i++) {
      if (this->reachable_consumers_[i].owner == owner) {
        if (this->reachable_consumers_[i].reachable != reachable) {
          this->reachable_consumers_[i].reachable = reachable;
          this->update_hm_count_sensor_();
        }
        return;
      }
    }
  }

  uint8_t get_registered_hm_count() const { return this->reachable_consumer_count_; }
  uint8_t get_reachable_hm_count() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < this->reachable_consumer_count_; i++) {
      if (this->reachable_consumers_[i].reachable) n++;
    }
    return n;
  }

  // ---------------------------------------------------------------------------
  // Public low-level API -- reserved for cooperating components (e.g. hm) when
  // external_mode is active.
  // ---------------------------------------------------------------------------
  uint8_t read_register(uint8_t reg) { return this->read_register_(reg); }
  void read_register(uint8_t reg, uint8_t *buf, uint8_t len) { this->read_register_(reg, buf, len); }
  uint8_t write_register(uint8_t reg, uint8_t value) { return this->write_register_(reg, value); }
  uint8_t write_register(uint8_t reg, const uint8_t *buf, uint8_t len) {
    return this->write_register_(reg, buf, len);
  }
  uint8_t get_status() { return this->get_status_(); }
  void flush_rx() { this->flush_rx_(); }
  void flush_tx() { this->flush_tx_(); }
  bool write_payload(const uint8_t *buf, uint8_t len) { return this->write_payload_(buf, len); }
  uint8_t read_payload(uint8_t *buf, uint8_t maxlen) { return this->read_payload_(buf, maxlen); }
  bool rx_available() { return this->rx_available_(); }
  void open_writing_pipe(const uint8_t *addr) {
    this->write_register_(REG_RX_ADDR_P0, addr, this->address_width_);
    this->write_register_(REG_TX_ADDR, addr, this->address_width_);
  }
  void open_reading_pipe(uint8_t pipe, const uint8_t *addr) { this->open_reading_pipe_(pipe, addr); }
  void start_listening() { this->start_listening_(); }
  void stop_listening() { this->stop_listening_(); }
  void set_channel_reg(uint8_t channel) { this->write_register_(REG_RF_CH, channel > 125 ? 125 : channel); }
  void set_retries_reg(uint8_t delay, uint8_t count) {
    this->write_register_(REG_SETUP_RETR, ((delay & 0x0F) << 4) | (count & 0x0F));
  }
  GPIOPin *get_ce_pin() { return this->ce_pin_; }
  /// To be called by an external_mode component that configures the radio's
  /// data rate directly via register (bypassing apply_data_rate_()) -- keeps
  /// the post-listen delay consistent with the real data rate used (see stop_listening_()).
  void set_tx_delay_us(uint32_t us) { this->tx_delay_us_ = us; }

  void setup() override;

  /// Hardware reset of the chip (power-down/flush/power-up cycle), without
  /// reconfiguration -- in external mode, call this followed by a full
  /// reconfiguration on the caller's side (e.g. hm::reset_radio()).
  bool reset_radio();
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Sends a packet (up to payload_size bytes). Blocking until TX_DS/MAX_RT
  /// or timeout_ms -- an nRF24 transmission is on the order of a few hundred
  /// microseconds to a few milliseconds depending on data_rate/retries, so a short
  /// block here is nowhere near as costly as the CMT2300A's bit-banged one.
  bool send_packet(const std::vector<uint8_t> &data, uint32_t timeout_ms = 100);

  void add_on_packet_received_callback(std::function<void(std::vector<uint8_t>)> cb) {
    this->packet_callback_.add(std::move(cb));
  }

 protected:
  // --- Low-level register access -- port of RF24::read_register/write_register ---
  uint8_t read_register_(uint8_t reg);
  void read_register_(uint8_t reg, uint8_t *buf, uint8_t len);
  uint8_t write_register_(uint8_t reg, uint8_t value);
  uint8_t write_register_(uint8_t reg, const uint8_t *buf, uint8_t len);
  uint8_t get_status_();
  void flush_rx_();
  void flush_tx_();

  // --- High-level sequence -- port of RF24::begin()/setXxx()/startListening()/... ---
  void power_up_();
  void apply_pa_level_();
  /// Reads back and logs a register at verbose level (post-write confirmation) --
  /// used during setup() to trace every register configured.
  void log_reg_(uint8_t reg, const char *label);
  void apply_data_rate_();
  void apply_crc_length_();
  void apply_retries_();
  void apply_address_width_();
  void open_writing_pipe_();
  void open_reading_pipe_(uint8_t pipe, const uint8_t *addr);
  void start_listening_();
  void stop_listening_();
  bool write_payload_(const uint8_t *buf, uint8_t len);
  uint8_t read_payload_(uint8_t *buf, uint8_t maxlen);
  bool rx_available_();

  GPIOPin *ce_pin_{nullptr};
  InternalGPIOPin *irq_pin_{nullptr};

  uint8_t channel_{76};
  PALevel pa_level_{PA_MAX};
  DataRate data_rate_{RATE_1MBPS};
  CRCLength crc_length_{CRC_16BIT};
  uint8_t address_width_{5};
  bool auto_ack_{true};
  uint8_t retry_delay_{5};
  uint8_t retry_count_{15};
  uint8_t payload_size_{32};
  bool dynamic_payloads_{false};
  uint8_t tx_address_[5]{0xE7, 0xE7, 0xE7, 0xE7, 0xE7};
  uint8_t rx_address_[5]{0xE7, 0xE7, 0xE7, 0xE7, 0xE7};

  bool listening_{false};
  bool setup_failed_{false};
  bool external_mode_{false};
  const void *external_lock_owner_{nullptr};
  uint32_t duty_lock_start_ms_{0};
  uint32_t duty_busy_accum_ms_{0};
  uint32_t duty_window_start_ms_{0};
  // Delay after ce(LOW) in stop_listening_(), before considering the radio
  // settled out of listening mode -- depends on the data rate (see apply_data_rate_()).
  // Values ported from RF24::_data_rate_reg_value() for F_CPU > 20MHz, which
  // unconditionally covers the ESP32.
  uint32_t tx_delay_us_{280};

  struct ReachableEntry {
    const void *owner{nullptr};
    bool reachable{false};
  };
  static const uint8_t MAX_REACHABLE_CONSUMERS = 8;
  ReachableEntry reachable_consumers_[MAX_REACHABLE_CONSUMERS]{};
  uint8_t reachable_consumer_count_{0};
#ifdef USE_SENSOR
  sensor::Sensor *hm_count_sensor_{nullptr};
#endif
#ifdef USE_SELECT
  select::Select *pa_level_select_{nullptr};
#endif

  void update_hm_count_sensor_() {
#ifdef USE_SENSOR
    if (this->hm_count_sensor_ != nullptr) this->hm_count_sensor_->publish_state(this->get_reachable_hm_count());
#endif
  }

  CallbackManager<void(std::vector<uint8_t>)> packet_callback_{};
};

template<typename... Ts> class NRF24SendAction : public Action<Ts...> {
 public:
  NRF24SendAction(NRF24Component *parent) : parent_(parent) {}

  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) {
    this->data_func_ = std::move(func);
    this->static_ = false;
  }
  void set_data_static(const std::vector<uint8_t> &data) {
    this->data_static_ = data;
    this->static_ = true;
  }

  void play(Ts... x) override {
    std::vector<uint8_t> data = this->static_ ? this->data_static_ : this->data_func_(x...);
    this->parent_->send_packet(data);
  }

 protected:
  NRF24Component *parent_;
  bool static_{true};
  std::vector<uint8_t> data_static_{};
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
};

class NRF24PacketReceivedTrigger : public Trigger<std::vector<uint8_t>> {
 public:
  explicit NRF24PacketReceivedTrigger(NRF24Component *parent) {
    parent->add_on_packet_received_callback([this](std::vector<uint8_t> data) { this->trigger(data); });
  }
};

}  // namespace nrf24l01
}  // namespace esphome
