#pragma once

#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

namespace esphome {
namespace cmt2300a {

// ---------------------------------------------------------------------------
// Registers and masks ported 1:1 from cmt2300a_defs.h (anisyanka/cmt2300a)
// ---------------------------------------------------------------------------
static const uint8_t REG_CUS_SYS2 = 0x0D;
static const uint8_t REG_CUS_PKT15 = 0x46;   // PAYLOAD_LENG (Tx and Rx)
static const uint8_t REG_CUS_PKT16 = 0x47;   // NODE_ERR_MASK, ...
static const uint8_t REG_CUS_PKT17 = 0x48;   // NODE_VALUE[7:0] -- also used for is_chip_exist()
static const uint8_t REG_CUS_PKT29 = 0x54;   // FIFO_TH
static const uint8_t REG_CUS_MODE_CTL = 0x60;
static const uint8_t REG_CUS_MODE_STA = 0x61;
static const uint8_t REG_CUS_FREQ_CHNL = 0x63;  // FH_CHANNEL -- channel register (fast hopping)
static const uint8_t REG_CUS_FREQ_OFS = 0x64;   // FH_OFFSET -- channel step (x 2.5kHz)
static const uint8_t REG_CUS_IO_SEL = 0x65;
static const uint8_t REG_CUS_INT1_CTL = 0x66;
static const uint8_t REG_CUS_INT2_CTL = 0x67;
static const uint8_t REG_CUS_INT_EN = 0x68;
static const uint8_t REG_CUS_FIFO_CTL = 0x69;
static const uint8_t REG_CUS_INT_CLR1 = 0x6A;
static const uint8_t REG_CUS_INT_CLR2 = 0x6B;
static const uint8_t REG_CUS_FIFO_CLR = 0x6C;
static const uint8_t REG_CUS_INT_FLAG = 0x6D;
static const uint8_t REG_CUS_FIFO_FLAG = 0x6E;
static const uint8_t REG_CUS_RSSI_CODE = 0x6F;
static const uint8_t REG_CUS_RSSI_DBM = 0x70;
static const uint8_t REG_NODE_ID_0 = 0x48;
static const uint8_t REG_NODE_ID_1 = 0x49;
static const uint8_t REG_NODE_ID_2 = 0x4A;
static const uint8_t REG_NODE_ID_3 = 0x4B;

// Default register banks (config_reg_bank in the reference driver)
static const uint8_t BANK_CMT_ADDR = 0x00;
static const uint8_t BANK_CMT_SIZE = 12;
static const uint8_t BANK_SYSTEM_ADDR = 0x0C;
static const uint8_t BANK_SYSTEM_SIZE = 12;
static const uint8_t BANK_FREQUENCY_ADDR = 0x18;
static const uint8_t BANK_FREQUENCY_SIZE = 8;
static const uint8_t BANK_DATA_RATE_ADDR = 0x20;
static const uint8_t BANK_DATA_RATE_SIZE = 24;
static const uint8_t BANK_BASEBAND_ADDR = 0x38;
static const uint8_t BANK_BASEBAND_SIZE = 29;
static const uint8_t BANK_TX_ADDR = 0x55;
static const uint8_t BANK_TX_SIZE = 11;

// Useful masks
static const uint8_t MASK_RSTN_IN_EN = 0x20;
static const uint8_t MASK_CFG_RETAIN = 0x10;
static const uint8_t MASK_CHIP_MODE_STA = 0x0F;
static const uint8_t MASK_LFOSC_RECAL_EN = 0x80;
static const uint8_t MASK_LFOSC_CAL1_EN = 0x40;
static const uint8_t MASK_LFOSC_CAL2_EN = 0x20;
static const uint8_t MASK_LFOSC_OUT_EN = 0x40;
static const uint8_t MASK_INT_POLAR = 0x20;
static const uint8_t MASK_INT1_SEL = 0x1F;
static const uint8_t MASK_INT2_SEL = 0x1F;
static const uint8_t MASK_NODE_ERR_MASK = 0x10;
static const uint8_t MASK_FIFO_TH = 0x7F;

static const uint8_t MASK_LBD_FLG = 0x80;
static const uint8_t MASK_COL_ERR_FLG = 0x40;
static const uint8_t MASK_PKT_ERR_FLG = 0x20;
static const uint8_t MASK_PREAM_OK_FLG = 0x10;
static const uint8_t MASK_SYNC_OK_FLG = 0x08;
static const uint8_t MASK_NODE_OK_FLG = 0x04;
static const uint8_t MASK_CRC_OK_FLG = 0x02;
static const uint8_t MASK_PKT_OK_FLG = 0x01;

static const uint8_t MASK_SL_TMO_FLG = 0x20;
static const uint8_t MASK_RX_TMO_FLG = 0x10;
static const uint8_t MASK_TX_DONE_FLG = 0x08;
static const uint8_t MASK_TX_DONE_CLR = 0x04;
static const uint8_t MASK_SL_TMO_CLR = 0x02;
static const uint8_t MASK_RX_TMO_CLR = 0x01;

static const uint8_t MASK_LBD_CLR = 0x20;
static const uint8_t MASK_PREAM_OK_CLR = 0x10;
static const uint8_t MASK_SYNC_OK_CLR = 0x08;
static const uint8_t MASK_NODE_OK_CLR = 0x04;
static const uint8_t MASK_CRC_OK_CLR = 0x02;
static const uint8_t MASK_PKT_DONE_CLR = 0x01;

static const uint8_t MASK_FIFO_RESTORE = 0x04;
static const uint8_t MASK_FIFO_CLR_RX = 0x02;
static const uint8_t MASK_FIFO_CLR_TX = 0x01;

static const uint8_t MASK_TX_DIN_EN = 0x80;
static const uint8_t MASK_FIFO_AUTO_CLR_DIS = 0x10;
static const uint8_t MASK_FIFO_TX_RD_EN = 0x08;
static const uint8_t MASK_FIFO_RX_TX_SEL = 0x04;
static const uint8_t MASK_FIFO_MERGE_EN = 0x02;
static const uint8_t MASK_SPI_FIFO_RD_WR_SEL = 0x01;

// cmt2300a_go_state_t
static const uint8_t GO_EEPROM = 0x01;
static const uint8_t GO_STBY = 0x02;
static const uint8_t GO_RFS = 0x04;
static const uint8_t GO_RX = 0x08;
static const uint8_t GO_SLEEP = 0x10;
static const uint8_t GO_TFS = 0x20;
static const uint8_t GO_TX = 0x40;

// cmt2300a_state_t
static const uint8_t STATE_IDLE = 0;
static const uint8_t STATE_SLEEP = 1;
static const uint8_t STATE_STBY = 2;
static const uint8_t STATE_RFS = 3;
static const uint8_t STATE_TFS = 4;
static const uint8_t STATE_RX = 5;
static const uint8_t STATE_TX = 6;
static const uint8_t STATE_INVALID = 0xFF;

// GPIOx_SEL (cmt2300a_gpio_functions_t) -- only what we actually use (INT1/INT2 on GPIO2/GPIO3)
static const uint32_t GPIO2_SEL_INT1 = 0x00;
static const uint32_t GPIO2_SEL_INT2 = 0x04;
static const uint32_t GPIO3_SEL_INT2 = 0x20;

// cmt2300_gpio_irq_mappings_t (what we actually use)
static const uint8_t INT_SEL_PKT_OK = 0x07;
static const uint8_t INT_SEL_TX_DONE = 0x0A;
static const uint8_t INT_SEL_PKT_DONE = 0x19;

// cmt2300_which_irq_enabled_t
static const uint8_t MASK_TX_DONE_EN = 0x20;
static const uint8_t MASK_CRC_OK_EN = 0x02;
static const uint8_t MASK_PKT_DONE_EN = 0x01;

enum class RadioState : uint8_t {
  IDLE = 0,
  RX_WAIT,
  TX_WAIT,
};

class CMT2300AComponent : public Component {
 public:
  void set_clk_pin(GPIOPin *pin) { this->clk_pin_ = pin; }
  void set_sdio_pin(GPIOPin *pin) { this->sdio_pin_ = pin; }
  void set_cs_pin(GPIOPin *pin) { this->cs_pin_ = pin; }
  void set_fcs_pin(GPIOPin *pin) { this->fcs_pin_ = pin; }
  void set_gpio2_pin(InternalGPIOPin *pin) { this->gpio2_pin_ = pin; }
  void set_gpio3_pin(InternalGPIOPin *pin) { this->gpio3_pin_ = pin; }
  void set_node_id(uint32_t node_id) { this->node_id_ = node_id; }
  void set_accept_any_node_id(bool accept) { this->accept_any_node_id_ = accept; }
  void set_fifo_threshold(uint8_t th) { this->fifo_threshold_ = th; }

  /// When true: setup() only does pin init + soft reset + chip detection.
  /// No "generic" register bank, no FIFO/IRQ init, no RX loop --
  /// the calling component (e.g. hms) drives everything via the low-level API below.
  /// Used when a third-party protocol (Hoymiles/HMS) needs its own register
  /// banks and its own Tx/Rx cadence, incompatible with generic mode.
  void set_external_mode(bool external) { this->external_mode_ = external; }
  bool get_external_mode() const { return this->external_mode_; }

  // ---------------------------------------------------------------------------
  // Arbitration for several "external_mode" components sharing the same chip
  // (e.g. several hms: on a single cmt2300a:). owner is an opaque identifier
  // (typically the caller's 'this').
  // ---------------------------------------------------------------------------
  /// Tries to take ownership of the chip. Returns true if acquired (or already
  /// held by this same owner); false if another component is currently using it.
  bool try_lock_external(const void *owner) {
    if (this->external_lock_owner_ == nullptr) {
      this->external_lock_owner_ = owner;
      this->duty_lock_start_ms_ = millis();
      return true;
    }
    if (this->external_lock_owner_ == owner) return true;
    return false;
  }
  /// Releases ownership -- only has an effect if 'owner' is indeed the current holder.
  void unlock_external(const void *owner) {
    if (this->external_lock_owner_ == owner) {
      this->duty_busy_accum_ms_ += millis() - this->duty_lock_start_ms_;
      this->external_lock_owner_ = nullptr;
    }
  }
  /// True if a component OTHER than 'owner' currently holds ownership of the chip.
  bool is_owned_by_other(const void *owner) const {
    return this->external_lock_owner_ != nullptr && this->external_lock_owner_ != owner;
  }

  /// Radio duty cycle (% of time spent with the lock held) since the last call,
  /// then resets the measurement window. Meant to be called periodically
  /// (e.g. from a sensor component), not continuously.
  float get_duty_cycle_percent_and_reset() {
    uint32_t now = millis();
    uint32_t window_ms = now - this->duty_window_start_ms_;
    uint32_t busy_ms = this->duty_busy_accum_ms_;
    if (this->external_lock_owner_ != nullptr) {
      // Lock currently held: count the portion of the ongoing exchange that
      // falls within this window, otherwise we'd underestimate the rate if the
      // window ends mid-exchange.
      busy_ms += now - this->duty_lock_start_ms_;
      this->duty_lock_start_ms_ = now;
    }
    this->duty_busy_accum_ms_ = 0;
    this->duty_window_start_ms_ = now;
    if (window_ms == 0) return 0.0f;
    return (static_cast<float>(busy_ms) / static_cast<float>(window_ms)) * 100.0f;
  }

  // ---------------------------------------------------------------------------
  // Tracking the number of "hms" components attached to this cmt2300a: that are
  // currently reachable. Each hms: registers itself once at startup, then
  // reports its reachable state on every change.
  // ---------------------------------------------------------------------------
#ifdef USE_SENSOR
  void set_hms_count_sensor(sensor::Sensor *s) { this->hms_count_sensor_ = s; }
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
          this->update_hms_count_sensor_();
        }
        return;
      }
    }
  }

  uint8_t get_registered_hms_count() const { return this->reachable_consumer_count_; }
  uint8_t get_reachable_hms_count() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < this->reachable_consumer_count_; i++) {
      if (this->reachable_consumers_[i].reachable) n++;
    }
    return n;
  }

  /// Lets several cooperating components know whether one of them has already
  /// done the full radio init (register banks, merged FIFO, etc.),
  /// to avoid redoing it for every new instance that starts up.
  bool is_external_radio_ready() const { return this->external_radio_ready_; }
  void set_external_radio_ready(bool ready) { this->external_radio_ready_ = ready; }

  /// Opaque identifier (defined by the calling component, e.g. hms's frequency
  /// band) of the "variant" the chip was initialized with, used to detect a
  /// mismatch if a second instance requests a different config.
  uint8_t get_external_radio_variant() const { return this->external_radio_variant_; }
  void set_external_radio_variant(uint8_t variant) { this->external_radio_variant_ = variant; }

  void setup() override;

  /// Hardware reset of the chip (chip soft reset command), without reconfiguration
  /// -- leaves the chip in its factory state. In external mode, call this followed
  /// by a full reconfiguration on the caller's side (e.g. hms::reset_radio()).
  bool reset_radio();

  /// Transmit power in dBm (-10 to +20), ported from CMT2300a::setPALevel()
  /// (dBm -> CUS_TX8/CUS_TX9 register lookup table + "double" bit in
  /// CUS_CMT4, verified directly against the real OpenDTU/anisyanka source).
  /// Optional: if never called, the fixed Tx bank ported from the reference
  /// config stays unchanged (historical behavior preserved).
  void set_pa_level(int8_t dbm);
  /// Called by YAML codegen to store the desired value -- the actual register
  /// write happens in setup(), after the fixed Tx bank.
  void configure_pa_level(int8_t dbm) {
    this->has_pa_level_ = true;
    this->pa_level_dbm_ = dbm;
  }
#ifdef USE_NUMBER
  void set_pa_level_number(number::Number *n) { this->pa_level_number_ = n; }
#endif
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Starts a non-blocking transmission. Returns false if the TX FIFO is too small
  /// or if the component is busy. Only valid in generic mode (external_mode=false).
  bool send_packet(const std::vector<uint8_t> &data, uint32_t timeout_ms = 200);

  void add_on_packet_received_callback(std::function<void(std::vector<uint8_t>)> cb) {
    this->packet_callback_.add(std::move(cb));
  }
  void add_on_tx_done_callback(std::function<void()> cb) { this->tx_done_callback_.add(std::move(cb)); }

  // ---------------------------------------------------------------------------
  // Public low-level API -- reserved for cooperating components (e.g. hms) when
  // external_mode is active. Direct access to the CMT2300A's registers/FIFO/state.
  // ---------------------------------------------------------------------------
  void write_reg(uint8_t reg, uint8_t data) { this->write_reg_(reg, data); }
  uint8_t read_reg(uint8_t reg) { return this->read_reg_(reg); }
  void write_fifo(const uint8_t *buf, size_t len) { this->write_fifo_(buf, len); }
  void read_fifo(uint8_t *buf, size_t len) { this->read_fifo_(buf, len); }
  void config_reg_bank(uint8_t base_addr, const uint8_t *bank, size_t len) {
    this->config_reg_bank_(base_addr, bank, len);
  }
  bool go_state(uint8_t go_state_mask, uint8_t desired_state) { return this->go_state_(go_state_mask, desired_state); }
  uint8_t get_state() { return this->get_state_(); }
  uint8_t clear_irq_flags() { return this->clear_irq_flags_(); }
  void fifo_write_enable() { this->fifo_write_enable_(); }
  void fifo_read_enable() { this->fifo_read_enable_(); }
  uint8_t fifo_clear_tx() { return this->fifo_clear_tx_(); }
  uint8_t fifo_clear_rx() { return this->fifo_clear_rx_(); }
  GPIOPin *get_sdio_pin() { return this->sdio_pin_; }
  InternalGPIOPin *get_gpio2_pin() { return this->gpio2_pin_; }
  InternalGPIOPin *get_gpio3_pin() { return this->gpio3_pin_; }

 protected:
  // --- Low-level bit-bang (port of if_send_byte/if_read_byte) ---
  void if_send_byte_(uint8_t data8);
  uint8_t if_read_byte_();
  void write_reg_(uint8_t reg, uint8_t data);
  uint8_t read_reg_(uint8_t reg);
  void write_fifo_(const uint8_t *buf, size_t len);
  void read_fifo_(uint8_t *buf, size_t len);
  void fifo_write_enable_();
  void fifo_read_enable_();
  uint8_t fifo_clear_tx_();
  uint8_t fifo_clear_rx_();
  void config_reg_bank_(uint8_t base_addr, const uint8_t *bank, size_t len);

  // --- High-level sequence (port of the cmt2300a_*() functions) ---
  bool soft_reset_();
  bool go_state_(uint8_t go_state_mask, uint8_t desired_state);
  uint8_t get_state_();
  bool is_chip_exist_();
  uint8_t clear_irq_flags_();
  bool select_gpio_pins_mode_(uint32_t mask);
  bool map_gpio_to_irq_(uint8_t int1_mapping, uint8_t int2_mapping);
  bool set_irq_polar_(bool level_high);
  bool enable_irq_(uint8_t mask);
  bool set_merge_fifo_(bool merged);
  bool set_fifo_threshold_reg_(uint8_t threshold);
  bool set_node_id_reg_(uint32_t node_id);
  bool allow_receiving_any_nodeid_(bool allow);
  bool start_receive_();

  static void gpio2_isr_(CMT2300AComponent *self);
  static void gpio3_isr_(CMT2300AComponent *self);

  GPIOPin *clk_pin_{nullptr};
  GPIOPin *sdio_pin_{nullptr};
  GPIOPin *cs_pin_{nullptr};
  GPIOPin *fcs_pin_{nullptr};
  InternalGPIOPin *gpio2_pin_{nullptr};
  InternalGPIOPin *gpio3_pin_{nullptr};

  uint32_t node_id_{0};
  bool accept_any_node_id_{false};
  uint8_t fifo_threshold_{32};

  bool is_fifo_merged_{false};
  uint8_t tx_fifo_size_{32};
  uint8_t rx_fifo_size_{32};

  RadioState state_{RadioState::IDLE};
  uint32_t state_deadline_{0};

  uint8_t rx_buf_[64];

  volatile bool tx_done_flag_{false};
  volatile bool rx_packet_flag_{false};

  bool setup_failed_{false};
  bool external_mode_{false};
  bool has_pa_level_{false};
  int8_t pa_level_dbm_{0};
#ifdef USE_NUMBER
  number::Number *pa_level_number_{nullptr};
#endif
  const void *external_lock_owner_{nullptr};
  uint32_t duty_lock_start_ms_{0};
  uint32_t duty_busy_accum_ms_{0};
  uint32_t duty_window_start_ms_{0};
  bool external_radio_ready_{false};
  uint8_t external_radio_variant_{0xFF};

  struct ReachableEntry {
    const void *owner{nullptr};
    bool reachable{false};
  };
  static const uint8_t MAX_REACHABLE_CONSUMERS = 8;
  ReachableEntry reachable_consumers_[MAX_REACHABLE_CONSUMERS]{};
  uint8_t reachable_consumer_count_{0};
#ifdef USE_SENSOR
  sensor::Sensor *hms_count_sensor_{nullptr};
#endif

  void update_hms_count_sensor_() {
#ifdef USE_SENSOR
    if (this->hms_count_sensor_ != nullptr) this->hms_count_sensor_->publish_state(this->get_reachable_hms_count());
#endif
  }

  CallbackManager<void(std::vector<uint8_t>)> packet_callback_{};
  CallbackManager<void()> tx_done_callback_{};
};

template<typename... Ts> class CMT2300ASendAction : public Action<Ts...> {
 public:
  CMT2300ASendAction(CMT2300AComponent *parent) : parent_(parent) {}

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
  CMT2300AComponent *parent_;
  bool static_{true};
  std::vector<uint8_t> data_static_{};
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
};

class CMT2300APacketReceivedTrigger : public Trigger<std::vector<uint8_t>> {
 public:
  explicit CMT2300APacketReceivedTrigger(CMT2300AComponent *parent) {
    parent->add_on_packet_received_callback([this](std::vector<uint8_t> data) { this->trigger(data); });
  }
};

class CMT2300ATxDoneTrigger : public Trigger<> {
 public:
  explicit CMT2300ATxDoneTrigger(CMT2300AComponent *parent) {
    parent->add_on_tx_done_callback([this]() { this->trigger(); });
  }
};

}  // namespace cmt2300a
}  // namespace esphome
