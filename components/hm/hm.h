#pragma once

#include <vector>
#include <string>
#include <cstring>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/nrf24l01/nrf24l01.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace hm {

// ---------------------------------------------------------------------------
// Types portés depuis OpenDTU lib/Hoymiles/src/parser/StatisticsParser.h
// (identiques à hms -- même famille de protocole Hoymiles) + CALC_CH_UDC,
// utilisé par les modèles HM 4 canaux (inverters/HM_4CH.cpp) mais pas par HMS.
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

enum {
  CALC_TOTAL_YT = 0,
  CALC_TOTAL_YD,
  CALC_CH_UDC,
  CALC_TOTAL_PDC,
  CALC_TOTAL_EFF,
};
static const uint16_t CMD_CALC = 0xFFFF;

struct byteAssign_t {
  ChannelType_t type;
  ChannelNum_t ch;
  FieldId_t fieldId;
  uint8_t start;  // position du 1er octet (ou index calc si div==CMD_CALC)
  uint8_t num;    // nombre d'octets (ou argument de la fonction calc)
  uint16_t div;   // diviseur, ou CMD_CALC
  bool isSigned;
  uint8_t digits;
};

static const uint8_t STATISTIC_PACKET_SIZE = 96;

// ---------------------------------------------------------------------------
// Fragment RF brut, porté depuis types.h (identique à hms)
// ---------------------------------------------------------------------------
struct fragment_t {
  uint8_t mainCmd{0};
  uint8_t fragment[32]{};
  uint8_t len{0};
  bool wasReceived{false};
};

static const uint8_t MAX_RF_FRAGMENT_COUNT = 6;

enum RadioOpState : uint8_t { OP_IDLE = 0, OP_WAIT_RESPONSE };
enum PendingCmd : uint8_t { CMD_NONE = 0, CMD_REALTIME_DATA, CMD_ACTIVE_POWER_CONTROL };
enum PowerLimitType : uint8_t { POWER_ABSOLUTE = 0, POWER_RELATIVE = 1 };

class HMComponent : public Component {
 public:
  void set_radio(nrf24l01::NRF24Component *radio) { this->radio_ = radio; }
  void set_inverter_serial(uint64_t serial) { this->inverter_serial_ = serial; }
  void set_dtu_serial(uint64_t serial) { this->dtu_serial_ = serial; }
  void set_poll_interval(uint32_t ms) { this->poll_interval_ms_ = ms; }

  void set_dc_power_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_power_[ch] = s; }
  void set_dc_current_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_current_[ch] = s; }
  void set_dc_voltage_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_voltage_[ch] = s; }
  void set_dc_energy_today_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_energy_today_[ch] = s; }
  void set_dc_energy_total_sensor(uint8_t ch, sensor::Sensor *s) { this->dc_energy_total_[ch] = s; }

  void set_ac_voltage_sensor(sensor::Sensor *s) { this->ac_voltage_ = s; }
  void set_ac_current_sensor(sensor::Sensor *s) { this->ac_current_ = s; }
  void set_ac_power_sensor(sensor::Sensor *s) { this->ac_power_ = s; }
  void set_ac_frequency_sensor(sensor::Sensor *s) { this->ac_frequency_ = s; }
  void set_ac_power_factor_sensor(sensor::Sensor *s) { this->ac_power_factor_ = s; }
  void set_ac_reactive_power_sensor(sensor::Sensor *s) { this->ac_reactive_power_ = s; }

  void set_inv_temperature_sensor(sensor::Sensor *s) { this->inv_temperature_ = s; }
  void set_inv_power_sensor(sensor::Sensor *s) { this->inv_power_ = s; }
  void set_inv_energy_today_sensor(sensor::Sensor *s) { this->inv_energy_today_ = s; }
  void set_inv_energy_total_sensor(sensor::Sensor *s) { this->inv_energy_total_ = s; }
  void set_inv_efficiency_sensor(sensor::Sensor *s) { this->inv_efficiency_ = s; }
  void set_reachable_sensor(binary_sensor::BinarySensor *s) { this->reachable_sensor_ = s; }
  void set_producing_sensor(binary_sensor::BinarySensor *s) { this->producing_sensor_ = s; }

  uint8_t get_dc_channel_count() const { return this->dc_channel_count_; }

  void set_power_limit_percent(float percent);
  void set_power_limit_absolute(float watts);
  void set_power_limit_percent_persistent(float percent);

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  bool decode_serial_();
  static uint64_t generate_dtu_serial_();
  bool init_radio_();

  // --- Hop de canal -- porté de HoymilesRadio_NRF.cpp ---
  uint8_t next_rx_channel_();
  uint8_t next_tx_channel_();
  void switch_rx_channel_();

  // --- Emission/réception bas niveau ---
  bool cmt_start_tx_(const uint8_t *buf, uint8_t len);  // nom conservé par cohérence avec hms
  void process_tx_();
  void open_reading_pipe_for_dtu_();
  void open_writing_pipe_for_inverter_();

  // --- Construction de trames -- portées de commands/*.cpp (identiques hms) ---
  void build_realtime_data_request_(uint8_t *out, uint8_t *out_len);
  void build_request_frame_(uint8_t frame_no, uint8_t *out, uint8_t *out_len);
  void build_active_power_control_(float limit, PowerLimitType type, bool persistent, uint8_t *out, uint8_t *out_len);

  void send_current_command_();
  void start_command_(PendingCmd cmd, const uint8_t *payload, uint8_t len, uint32_t timeout_ms);

  void clear_rx_fragment_buffer_();
  void add_rx_fragment_(const uint8_t *fragment, uint8_t len);
  uint8_t verify_all_fragments_();
  bool handle_realtime_response_();
  void publish_sensors_();
  void publish_reachable_();

  const byteAssign_t *find_assignment_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const;
  float get_field_value_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const;

  nrf24l01::NRF24Component *radio_{nullptr};

  uint64_t inverter_serial_{0};
  uint64_t dtu_serial_{0};
  uint32_t poll_interval_ms_{5000};

  uint8_t dc_channel_count_{0};
  std::string type_name_;
  const byteAssign_t *byte_assignment_{nullptr};
  uint8_t byte_assignment_size_{0};
  uint8_t expected_byte_count_{0};

  uint8_t rx_ch_list_[5]{3, 23, 40, 61, 75};
  uint8_t tx_ch_list_[5]{3, 23, 40, 61, 75};
  uint8_t rx_ch_idx_{0};
  uint8_t tx_ch_idx_{0};
  uint32_t last_rx_switch_ms_{0};

  uint8_t stats_buf_[STATISTIC_PACKET_SIZE]{};
  bool has_valid_stats_{false};

  fragment_t rx_fragments_[MAX_RF_FRAGMENT_COUNT]{};
  uint8_t rx_fragment_last_id_{0};
  uint8_t rx_fragment_max_id_{0};
  uint8_t rx_retransmit_count_{0};

  PendingCmd pending_cmd_{CMD_NONE};
  RadioOpState op_state_{OP_IDLE};
  uint8_t tx_payload_[32]{};
  uint8_t tx_payload_len_{0};
  uint8_t send_count_{0};
  uint32_t cmd_deadline_{0};

  bool tx_sending_{false};
  uint32_t tx_start_{0};

  uint32_t rx_failure_count_{0};
  static const uint8_t REACHABLE_THRESHOLD = 3;

  bool power_limit_pending_{false};
  float power_limit_value_{100.0f};
  PowerLimitType power_limit_type_{POWER_RELATIVE};
  bool power_limit_persistent_{false};

  uint32_t last_poll_{0};

  sensor::Sensor *dc_power_[4]{};
  sensor::Sensor *dc_current_[4]{};
  sensor::Sensor *dc_voltage_[4]{};
  sensor::Sensor *dc_energy_today_[4]{};
  sensor::Sensor *dc_energy_total_[4]{};

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
  binary_sensor::BinarySensor *reachable_sensor_{nullptr};
  binary_sensor::BinarySensor *producing_sensor_{nullptr};
};

}  // namespace hm
}  // namespace esphome
