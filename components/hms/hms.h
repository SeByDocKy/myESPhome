#pragma once

#include <vector>
#include <string>
#include <cstring>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/cmt2300a/cmt2300a.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome {
namespace hms {

// ---------------------------------------------------------------------------
// Types ported from OpenDTU lib/Hoymiles/src/parser/StatisticsParser.h
// (only the subset used by HMS 1/2/4-channel inverters)
// ---------------------------------------------------------------------------
enum FieldId_t : uint8_t {
  FLD_UDC = 0,
  FLD_IDC,
  FLD_PDC,
  FLD_YD,
  FLD_YT,
  FLD_UAC,
  FLD_IAC,
  FLD_PAC,
  FLD_F,
  FLD_T,
  FLD_PF,
  FLD_EFF,
  FLD_IRR,
  FLD_Q,
  FLD_EVT_LOG,
};

enum ChannelType_t : uint8_t { TYPE_AC = 0, TYPE_DC, TYPE_INV };
enum ChannelNum_t : uint8_t { CH0 = 0, CH1, CH2, CH3 };

// Calc function indices (when div == CMD_CALC)
enum {
  CALC_TOTAL_YT = 0,
  CALC_TOTAL_YD,
  CALC_TOTAL_PDC,
  CALC_TOTAL_EFF,
  CALC_CH_IRR,
};
static const uint16_t CMD_CALC = 0xFFFF;

struct byteAssign_t {
  ChannelType_t type;
  ChannelNum_t ch;
  FieldId_t fieldId;
  uint8_t start;   // position of the 1st byte in the buffer (or calc index if div==CMD_CALC)
  uint8_t num;     // number of bytes (or the calc function's argument)
  uint16_t div;    // divisor, or CMD_CALC
  bool isSigned;
  uint8_t digits;
};

static const uint8_t STATISTIC_PACKET_SIZE = 7 * 16;  // 112 bytes, same as OpenDTU

// ---------------------------------------------------------------------------
// Frequency / country -- ported from HoymilesRadio_CMT.h/cpp
// ---------------------------------------------------------------------------
enum class FrequencyBand : uint8_t { EU_860 = 0, US_900 };

struct FreqDef {
  uint32_t base_freq;      // Hz -- band base (860 or 900 MHz)
  uint32_t freq_startup;   // Hz -- inverter's "boot" frequency after a power loss
  uint32_t freq_default;   // Hz -- DTU's default work frequency
};

static const uint32_t CMT_ONE_STEP_SIZE = 2500;  // Hz, one step = 2.5 kHz
static const uint8_t FH_OFFSET = 100;            // step * FH_OFFSET = channel width
// getChannelWidth() = FH_OFFSET * CMT_ONE_STEP_SIZE = 250 kHz

// ---------------------------------------------------------------------------
// Raw RF fragment (32 bytes max), ported from types.h
// ---------------------------------------------------------------------------
struct fragment_t {
  uint8_t mainCmd{0};
  uint8_t fragment[32]{};
  uint8_t len{0};
  bool wasReceived{false};
};

// OpenDTU uses 13 to cover all its inverter types (including the larger
// three-phase HMT ones). HMS models never need more than 4-5 fragments
// (observed 3 in a real test on an HMS-2CH) -- reduced here to save static
// RAM, with a comfortable margin.
static const uint8_t MAX_RF_FRAGMENT_COUNT = 6;

enum RadioOpState : uint8_t {
  OP_IDLE = 0,
  OP_WAIT_RESPONSE,
};

// Logical identifier of the command currently in flight (to know how to
// interpret the response / what to do on timeout)
enum PendingCmd : uint8_t {
  CMD_NONE = 0,
  CMD_REALTIME_DATA,
  CMD_ACTIVE_POWER_CONTROL,
  CMD_CHANNEL_CHANGE,
};

enum PowerLimitType : uint8_t {
  POWER_ABSOLUTE = 0,
  POWER_RELATIVE = 1,
};

class HMSComponent : public Component {
 public:
  void set_radio(cmt2300a::CMT2300AComponent *radio) { this->radio_ = radio; }
  void set_inverter_serial(uint64_t serial) { this->inverter_serial_ = serial; }
  void set_dtu_serial(uint64_t serial) { this->dtu_serial_ = serial; }
  void set_frequency_band(uint8_t band) { this->frequency_band_ = static_cast<FrequencyBand>(band); }
  void set_poll_interval(uint32_t ms) { this->poll_interval_ms_ = ms; }
  void set_realtime_timeout(uint32_t ms) { this->realtime_timeout_ms_ = ms; }
  void set_power_control_timeout(uint32_t ms) { this->power_control_timeout_ms_ = ms; }

#ifdef USE_SENSOR
  // --- DC sensors (up to 4 channels depending on the model decoded from the SN) ---
  void set_dc_power_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_power_[ch] = s; }
  void set_dc_current_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_current_[ch] = s; }
  void set_dc_voltage_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_voltage_[ch] = s; }
  void set_dc_energy_today_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_energy_today_[ch] = s; }
  void set_dc_energy_total_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_energy_total_[ch] = s; }
  void set_dc_irradiation_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_irradiation_[ch] = s; }

  // --- AC sensors (single channel 0, HMS = single-phase) ---
  void set_ac_voltage_sensor(sensor::Sensor *s) { this->ac_voltage_ = s; }
  void set_ac_current_sensor(sensor::Sensor *s) { this->ac_current_ = s; }
  void set_ac_power_sensor(sensor::Sensor *s) { this->ac_power_ = s; }
  void set_ac_frequency_sensor(sensor::Sensor *s) { this->ac_frequency_ = s; }
  void set_ac_power_factor_sensor(sensor::Sensor *s) { this->ac_power_factor_ = s; }
  void set_ac_reactive_power_sensor(sensor::Sensor *s) { this->ac_reactive_power_ = s; }

  // --- Inverter sensors (aggregates) ---
  void set_inv_temperature_sensor(sensor::Sensor *s) { this->inv_temperature_ = s; }
  void set_inv_power_sensor(sensor::Sensor *s) { this->inv_power_ = s; }
  void set_inv_energy_today_sensor(sensor::Sensor *s) { this->inv_energy_today_ = s; }
  void set_inv_energy_total_sensor(sensor::Sensor *s) { this->inv_energy_total_ = s; }
  void set_inv_efficiency_sensor(sensor::Sensor *s) { this->inv_efficiency_ = s; }
  void set_rssi_sensor(sensor::Sensor *s) { this->rssi_sensor_ = s; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_reachable_sensor(binary_sensor::BinarySensor *s) { this->reachable_sensor_ = s; }
  void set_producing_sensor(binary_sensor::BinarySensor *s) { this->producing_sensor_ = s; }
#endif

  uint8_t get_dc_channel_count() const { return this->dc_channel_count_; }

  /// Called by the number platform: power limit in % (0-100, relative, non-persistent)
  void set_power_limit_percent(float percent);
  /// Power limit in absolute Watts (non-persistent)
  void set_power_limit_absolute(float watts);
  /// Power limit in % (0-100, relative), written PERSISTENT (stored in
  /// the inverter's EEPROM, survives power cycles). Only use
  /// occasionally (flash wear) -- typically via the
  /// reset_to_output_min/reset_to_output_max buttons, not for frequent control.
  void set_power_limit_percent_persistent(float percent);

  /// Hardware reset of the chip + full Hoymiles reconfiguration, without rebooting
  /// the ESP32. Blocking (up to ~400ms, similar to the initial setup()) -- a rare
  /// manual action, not a routine code path.
  void reset_radio();

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  // --- Serial number decoding -> model / byte table ---
  bool decode_serial_();
  static uint64_t generate_dtu_serial_();

  // --- Hoymiles-specific radio init (860/900MHz banks, merged FIFO, IRQ) ---
  bool init_radio_();
  void switch_to_frequency_(uint32_t freq_hz);
  void switch_to_channel_(uint8_t channel);
  uint32_t frequency_from_channel_(uint8_t channel) const;
  uint8_t channel_from_frequency_(uint32_t freq_hz) const;

  // --- Low-level Tx/Rx (ported from cmt2300wrapper.cpp) ---
  bool cmt_start_tx_(const uint8_t *buf, uint8_t len);
  void process_tx_();
  bool cmt_start_listening_();
  bool cmt_rx_packet_available_();
  uint8_t cmt_read_dynamic_payload_(uint8_t *buf, uint8_t maxlen);

  // --- Frame construction (ported from commands/*.cpp) ---
  void build_realtime_data_request_(uint8_t *out, uint8_t *out_len);
  void build_request_frame_(uint8_t frame_no, uint8_t *out, uint8_t *out_len);
  void build_active_power_control_(float limit, PowerLimitType type, bool persistent, uint8_t *out, uint8_t *out_len);
  void build_channel_change_(uint8_t channel, uint8_t *out, uint8_t *out_len);

  void send_current_command_();
  void start_command_(PendingCmd cmd, const uint8_t *payload, uint8_t len, uint32_t timeout_ms);

  // --- Fragment reassembly / verification (ported from InverterAbstract) ---
  void clear_rx_fragment_buffer_();
  void add_rx_fragment_(const uint8_t *fragment, uint8_t len);
  uint8_t verify_all_fragments_();  // FRAGMENT_OK(0) / id to retransmit / error codes
  bool handle_realtime_response_();
  void apply_statistics_buffer_();
  void publish_sensors_();
  void publish_reachable_();

  // --- Access to decoded values (ported from StatisticsParser::getChannelFieldValue) ---
  const byteAssign_t *find_assignment_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const;
  float get_field_value_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const;
  bool has_field_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const;

  cmt2300a::CMT2300AComponent *radio_{nullptr};

  uint64_t inverter_serial_{0};
  uint64_t dtu_serial_{0};
  FrequencyBand frequency_band_{FrequencyBand::EU_860};
  uint32_t poll_interval_ms_{5000};
  // Delays before giving up/retransmitting a command -- default values
  // identical to OpenDTU's (RealTimeRunDataCommand::setTimeout(500),
  // ActivePowerControlCommand::setTimeout(2000)), exposed in YAML to
  // allow future calibration without recompiling the component.
  uint32_t realtime_timeout_ms_{500};
  uint32_t power_control_timeout_ms_{2000};

  uint8_t dc_channel_count_{0};
  std::string type_name_;
  const byteAssign_t *byte_assignment_{nullptr};
  uint8_t byte_assignment_size_{0};
  uint8_t expected_byte_count_{0};

  uint32_t work_frequency_{0};
  uint8_t work_channel_{0};

  uint8_t stats_buf_[STATISTIC_PACKET_SIZE]{};
  bool has_valid_stats_{false};

  // Reassembly of the current response's fragments
  fragment_t rx_fragments_[MAX_RF_FRAGMENT_COUNT]{};
  uint8_t rx_fragment_last_id_{0};
  uint8_t rx_fragment_max_id_{0};
  uint8_t rx_retransmit_count_{0};

  // Command currently in flight
  PendingCmd pending_cmd_{CMD_NONE};
  RadioOpState op_state_{OP_IDLE};
  uint8_t tx_payload_[32]{};
  uint8_t tx_payload_len_{0};
  uint8_t send_count_{0};
  uint32_t cmd_deadline_{0};

  // Non-blocking Tx: the chip transmits in the background, process_tx_()
  // (called on every loop() tick) checks TX_DONE without ever actively waiting.
  bool tx_sending_{false};
  uint32_t tx_start_{0};
  bool tx_restore_channel_{false};
  uint8_t tx_restore_channel_value_{0};
  bool tx_release_lock_after_{false};

  // Link reliability (to trigger a ChannelChangeCommand)
  uint32_t rx_failure_count_{0};
  static const uint8_t REACHABLE_THRESHOLD = 3;

  // Pending power setting (sent as soon as the radio channel is free)
  bool power_limit_pending_{false};
  float power_limit_value_{100.0f};
  PowerLimitType power_limit_type_{POWER_RELATIVE};
  bool power_limit_persistent_{false};

  uint32_t last_poll_{0};
  int8_t last_rssi_dbm_{-127};

#ifdef USE_SENSOR
  sensor::Sensor *dc_power_[4]{};
  sensor::Sensor *dc_current_[4]{};
  sensor::Sensor *dc_voltage_[4]{};
  sensor::Sensor *dc_energy_today_[4]{};
  sensor::Sensor *dc_energy_total_[4]{};
  sensor::Sensor *dc_irradiation_[4]{};

  sensor::Sensor *ac_voltage_{nullptr};
  sensor::Sensor *ac_current_{nullptr};
  sensor::Sensor *ac_power_{nullptr};
  sensor::Sensor *ac_frequency_{nullptr};
  sensor::Sensor *ac_power_factor_{nullptr};
  sensor::Sensor *ac_reactive_power_{nullptr};

  sensor::Sensor *inv_temperature_{nullptr};
  sensor::Sensor *inv_power_{nullptr};
  sensor::Sensor *inv_energy_today_{nullptr};
  sensor::Sensor *inv_energy_total_{nullptr};
  sensor::Sensor *inv_efficiency_{nullptr};
  sensor::Sensor *rssi_sensor_{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *reachable_sensor_{nullptr};
  binary_sensor::BinarySensor *producing_sensor_{nullptr};
#endif
};

}  // namespace hms
}  // namespace esphome
