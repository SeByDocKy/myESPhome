#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

#include "aps_protocol.h"
#ifdef USE_APSZB_NATIVE
#include "aps_radio.h"
#endif
#ifdef USE_APSZB_ZNP
#include "znp_radio.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/gpio.h"
#endif

#include <deque>
#include <string>
#include <vector>

namespace esphome {
namespace apszb {

class APSZBComponent;

// ---------------------------------------------------------------------------------------------------------------------
// One APsystems micro-inverter (YC600 / QS1 / DS3)
// ---------------------------------------------------------------------------------------------------------------------
class APSInverter {
 public:
  // ---- configuration ----
  void set_serial(const std::string &hex);
  void set_type(uint8_t type) { this->type_ = static_cast<InverterType>(type); }
  void set_short_address(uint16_t addr) {
    this->configured_short_ = addr;
    this->short_addr_ = addr;
  }
  void set_calibration(int watts) { this->calibration_ = watts; }
  void set_rated_power(uint16_t watts) { this->rated_power_ = watts; }
  void set_parent(APSZBComponent *parent, uint8_t index) {
    this->parent_ = parent;
    this->index_ = index;
  }

#ifdef USE_SENSOR
  void set_ac_voltage_sensor(sensor::Sensor *s) { this->ac_voltage_ = s; }
  void set_frequency_sensor(sensor::Sensor *s) { this->frequency_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_ = s; }
  void set_link_quality_sensor(sensor::Sensor *s) { this->link_quality_ = s; }
  void set_rssi_sensor(sensor::Sensor *s) { this->rssi_ = s; }
  void set_power_total_sensor(sensor::Sensor *s) { this->power_total_ = s; }
  void set_energy_total_sensor(sensor::Sensor *s) { this->energy_total_ = s; }
  void set_uptime_sensor(sensor::Sensor *s) { this->uptime_ = s; }
  void set_dc_voltage_sensor(uint8_t ch, sensor::Sensor *s) {
    if (ch < 4)
      this->dc_voltage_[ch] = s;
  }
  void set_dc_current_sensor(uint8_t ch, sensor::Sensor *s) {
    if (ch < 4)
      this->dc_current_[ch] = s;
  }
  void set_power_sensor(uint8_t ch, sensor::Sensor *s) {
    if (ch < 4)
      this->power_[ch] = s;
  }
  void set_energy_sensor(uint8_t ch, sensor::Sensor *s) {
    if (ch < 4)
      this->energy_[ch] = s;
  }
#endif
#ifdef USE_TEXT_SENSOR
  void set_status_text_sensor(text_sensor::TextSensor *s) { this->status_text_ = s; }
  void set_serial_text_sensor(text_sensor::TextSensor *s) { this->serial_text_ = s; }
  void set_short_address_text_sensor(text_sensor::TextSensor *s) { this->short_address_text_ = s; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_online_binary_sensor(binary_sensor::BinarySensor *s) { this->online_ = s; }
#endif
#ifdef USE_NUMBER
  void set_max_power_number(number::Number *n) { this->max_power_number_ = n; }
#endif

  // ---- user actions (buttons / number) ----
  void request_pair();
  void request_refresh();
  void request_query();
  void request_reboot();
  void request_max_power(float watts);

  // ---- accessors ----
  const uint8_t *serial() const { return this->serial_; }
  const std::string &serial_hex() const { return this->serial_hex_; }
  InverterType type() const { return this->type_; }
  uint16_t short_address() const { return this->short_addr_; }
  bool paired() const { return this->short_addr_ != 0 && this->short_addr_ < 0xFFF8; }
  bool online() const { return this->online_state_; }
  int calibration() const { return this->calibration_; }
  uint16_t rated_power() const { return this->rated_power_; }
  uint8_t index() const { return this->index_; }

  // ---- called by the hub ----
  void setup_();
  void dump_config_();
  void on_poll_(const PollData &d, int8_t rssi, uint8_t lqi);
  void on_poll_failed_();
  void on_max_power_(float watts_with_calibration);
  void on_paired_(uint16_t short_addr);
  void publish_status_();
  void save_energy_if_needed_(uint32_t now);

 protected:
  struct Persist {
    uint16_t magic;
    uint16_t short_addr;
    float energy_total_wh;
  };
  void save_();
  void zero_power_();

  APSZBComponent *parent_{nullptr};
  uint8_t index_{0};
  uint8_t serial_[6]{0};
  std::string serial_hex_;
  InverterType type_{InverterType::YC600};
  uint16_t configured_short_{0};
  uint16_t short_addr_{0};
  int calibration_{0};
  uint16_t rated_power_{600};  // W, reference of the power_limit_percent output (set from the YAML)

  // run-time state
  bool online_state_{false};
  uint8_t fail_count_{0};
  bool have_prev_{false};
  uint32_t prev_ts_{0};
  float prev_energy_[4]{0};
  float energy_total_wh_{0};
  float saved_energy_wh_{0};
  uint32_t last_save_{0};
  std::string last_status_;
  ESPPreferenceObject pref_;

#ifdef USE_SENSOR
  sensor::Sensor *ac_voltage_{nullptr};
  sensor::Sensor *frequency_{nullptr};
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *link_quality_{nullptr};
  sensor::Sensor *rssi_{nullptr};
  sensor::Sensor *power_total_{nullptr};
  sensor::Sensor *energy_total_{nullptr};
  sensor::Sensor *uptime_{nullptr};
  sensor::Sensor *dc_voltage_[4]{nullptr, nullptr, nullptr, nullptr};
  sensor::Sensor *dc_current_[4]{nullptr, nullptr, nullptr, nullptr};
  sensor::Sensor *power_[4]{nullptr, nullptr, nullptr, nullptr};
  sensor::Sensor *energy_[4]{nullptr, nullptr, nullptr, nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *status_text_{nullptr};
  text_sensor::TextSensor *serial_text_{nullptr};
  text_sensor::TextSensor *short_address_text_{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *online_{nullptr};
#endif
#ifdef USE_NUMBER
  number::Number *max_power_number_{nullptr};
#endif
};

// ---------------------------------------------------------------------------------------------------------------------
// Hub: owns the 802.15.4 radio and talks to all inverters (replaces the CC2530 ZNP modem of the original project)
// ---------------------------------------------------------------------------------------------------------------------
enum class OpKind : uint8_t { POLL, QUERY, THROTTLE, REBOOT, NORMAL_OPS, PAIR };

class APSZBComponent : public Component {
 public:
  // ---- configuration ----
  void set_ecu_id(const std::string &hex) { this->ecu_id_hex_ = hex; }
  void set_channel(uint8_t channel) { this->cfg_.channel = channel; }
  void set_tx_power(int8_t dbm) { this->tx_power_ = dbm; }
  void set_poll_interval(uint32_t ms) { this->poll_interval_ = ms; }
  void set_response_timeout(uint32_t ms) { this->response_timeout_ = ms; }
  void set_link_status_interval(uint32_t ms) { this->link_status_interval_ = ms; }
  void set_route_request_interval(uint32_t ms) { this->route_request_interval_ = ms; }
  void set_auto_pair(bool v) { this->auto_pair_ = v; }
  void set_promiscuous(bool v) { this->promiscuous_ = v; }
  void set_log_frames(bool v) { this->log_frames_ = v; }
  void set_nwk_protocol_version(uint8_t v) { this->cfg_.nwk_protocol_version = v; }
  void set_discover_route(bool v) { this->cfg_.discover_route = v; }
  void set_mac_ack(bool v) { this->cfg_.mac_ack = v; }
#ifdef USE_APSZB_ZNP
  void set_uart(uart::UARTComponent *uart) { this->uart_ = uart; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
#endif
  void set_offline_after(uint8_t n) { this->offline_after_ = n; }
  uint8_t offline_after() const { return this->offline_after_; }
  void add_inverter(APSInverter *inv) {
    inv->set_parent(this, uint8_t(this->inverters_.size()));
    this->inverters_.push_back(inv);
  }

  // ---- Component ----
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // ---- operations requested by the inverters / buttons ----
  void enqueue_op(OpKind kind, uint8_t inv, int32_t arg = 0);

 protected:
  struct Step {
    enum Type : uint8_t { SEND, DELAY, AWAIT } type{DELAY};
    TxFrame frame;
    uint32_t ms{0};
  };
  struct Op {
    OpKind kind;
    uint8_t inv;
    int32_t arg;
  };
  struct TxJob {
    TxFrame frame;
    uint32_t id{0};
    uint8_t attempts{0};
  };

  // radio / frames
#ifdef USE_APSZB_NATIVE
  void handle_rx_(const RadioRx &rx);
#endif
#ifdef USE_APSZB_ZNP
  void handle_znp_rx_(const ZnpRx &rx);
#endif
  bool radio_ready_() const;
  bool radio_transmit_(const TxFrame &f);
  int radio_tx_result_() const;
  const char *radio_tx_error_(int code) const;
  void handle_asdu_(const RxFrame &f, const uint8_t *asdu, size_t n, int8_t rssi, uint8_t lqi);
  void process_tx_(uint32_t now);
  uint32_t queue_tx_(const TxFrame &f);

  // operations
  void run_op_(uint32_t now);
  void start_op_(const Op &op);
  void build_steps_();
  void finish_op_(bool ok);
  void add_unicast_(uint16_t dst, uint16_t cluster, const uint8_t *asdu, size_t n);
  void add_broadcast_(uint16_t pan, uint16_t cluster, const uint8_t *asdu, size_t n);
  void add_delay_(uint32_t ms);
  void add_await_(uint32_t ms);

  void housekeeping_(uint32_t now);
  bool op_queued_(OpKind kind, uint8_t inv) const;
  int find_inverter_(const uint8_t *asdu, size_t n, bool anywhere, size_t *offset) const;
  bool plausible_(const PollData &d) const;

  std::vector<APSInverter *> inverters_;
  NetworkConfig cfg_;
  FrameFactory factory_{&this->cfg_};
  Reassembler reasm_;
#ifdef USE_APSZB_NATIVE
  ApsRadio radio_;
#endif
#ifdef USE_APSZB_ZNP
  // adapter between the ZNP client and an ESPHome UART (+ optional RESET pin of the CC2530)
  class UartPort : public ZnpPort {
   public:
    void write(const uint8_t *data, size_t len) override { this->uart->write_array(data, len); }
    int read_byte() override {
      uint8_t b;
      if (this->uart->available() == 0 || !this->uart->read_byte(&b))
        return -1;
      return int(b);
    }
    void set_reset(bool asserted) override {
      if (this->reset != nullptr)
        this->reset->digital_write(!asserted);  // RESET_N is active low
    }
    bool has_reset() const override { return this->reset != nullptr; }
    uart::UARTComponent *uart{nullptr};
    GPIOPin *reset{nullptr};
  };
  ZnpRadio znp_;
  UartPort znp_port_;
  uart::UARTComponent *uart_{nullptr};
  GPIOPin *reset_pin_{nullptr};
#endif
  uint8_t ecu_rev_[6]{0};

  // configuration
  std::string ecu_id_hex_;
  int8_t tx_power_{10};
  uint32_t poll_interval_{60000};
  uint32_t response_timeout_{2500};
  uint32_t link_status_interval_{15000};
  uint32_t route_request_interval_{60000};
  uint8_t offline_after_{3};
  bool auto_pair_{true};
  bool promiscuous_{false};
  bool log_frames_{false};

  // timers
  uint32_t next_poll_{0};
  uint32_t next_link_status_{0};
  uint32_t next_rreq_{0};
  uint32_t next_normal_ops_{0};
  uint32_t next_pair_try_{0};
  uint32_t start_time_{0};
  bool normal_ops_sent_{false};
  bool warned_secured_{false};

  // TX engine
  std::deque<TxJob> txq_;
  TxJob cur_tx_;
  bool tx_active_{false};
  uint32_t tx_started_{0};
  uint32_t tx_not_before_{0};
  uint32_t tx_id_counter_{0};
  uint32_t tx_done_id_{0};
  bool last_tx_ok_{true};

  // operation engine
  std::deque<Op> ops_;
  Op cur_{OpKind::POLL, 0, 0};
  bool op_active_{false};
  bool op_ok_{false};
  std::vector<Step> steps_;
  size_t step_idx_{0};
  uint32_t step_started_{0};
  bool send_issued_{false};
  uint32_t send_job_id_{0};
  bool got_response_{false};
  uint8_t attempt_{0};
  uint16_t pair_addr_{0};
};

}  // namespace apszb
}  // namespace esphome
