#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_OUTPUT
#include "esphome/components/output/float_output.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif

#include <memory>
#include <string>
#include <vector>

namespace esphome {
namespace tsungen3 {

// ---------------------------------------------------------------------------
// Solarman V5 framing constants
// ---------------------------------------------------------------------------
static const uint8_t V5_START = 0xA5;
static const uint8_t V5_END = 0x15;
static const uint16_t V5_CTRL_REQUEST = 0x4510;
static const uint16_t V5_CTRL_RESPONSE = 0x1510;

// Modbus function codes
static const uint8_t MB_READ_HOLDING_REGISTERS = 0x03;
static const uint8_t MB_WRITE_SINGLE_REGISTER = 0x06;

// Solarman V5 "Frame Type" byte (payload offset 0)
static const uint8_t V5_FRAME_TYPE_INVERTER = 0x02;
static const uint8_t V5_FRAME_TYPE_AT_CMD = 0x01;
static const uint8_t V5_FRAME_TYPE_AT_CMD_RSP = 0x08;
// "Sensor Type" field: 0x0000 for plain Modbus polling, 0x0002 for AT+ commands
// -- confirmed against s-allius/tsun-gen3-proxy's gen3plus/solarman_v5.py
// (send_at_cmd()/AT_CMD framing), not just the generic pysolarmanv5 spec.
static const uint16_t V5_SENSOR_TYPE_MODBUS = 0x0000;
static const uint16_t V5_SENSOR_TYPE_AT_CMD = 0x0002;

// Live-data register block (see s-allius/tsun-gen3-proxy wiki: MODBUS registers)
static const uint16_t REG_BLOCK_START = 0x3000;
static const uint16_t REG_BLOCK_COUNT = 0x2A;  // 0x3000 .. 0x3029 inclusive

// "Output Coefficient" register (0x2000 config block). Ratio 100/1024, i.e.
// register_value = percent * 1024 / 100. NOT independently verified against a
// packet capture -- taken from the tsun-gen3-proxy wiki's MODBUS register
// table and its "inverter-output-coefficient" release note (v0.9.0).
static const uint16_t REG_OUTPUT_COEFFICIENT = 0x202C;

class TSunGen3Component : public PollingComponent {
 public:
  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_modbus_address(uint8_t address) { this->modbus_address_ = address; }
  void set_logger_serial(uint32_t serial) { this->logger_serial_ = serial; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Writes the Output Coefficient register (percent of Rated Power, 0-100).
  // Called by the `number` and `output` platforms. Fire-and-forget: logs on
  // failure, does not throw/block the caller beyond the usual TCP timeout.
  void set_power_percent(float percent);

  // Sends "AT+Z" (Re-start module) and logs whatever the device replies.
  // Called by the `button` platform.
  void send_reset_command();

#ifdef USE_SENSOR
  void set_grid_voltage_sensor(sensor::Sensor *s) { this->grid_voltage_sensor_ = s; }
  void set_grid_current_sensor(sensor::Sensor *s) { this->grid_current_sensor_ = s; }
  void set_grid_frequency_sensor(sensor::Sensor *s) { this->grid_frequency_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }
  void set_rated_power_sensor(sensor::Sensor *s) { this->rated_power_sensor_ = s; }
  void set_current_power_sensor(sensor::Sensor *s) { this->current_power_sensor_ = s; }
  void set_ac_energy_today_sensor(sensor::Sensor *s) { this->ac_energy_today_sensor_ = s; }
  void set_ac_energy_total_sensor(sensor::Sensor *s) { this->ac_energy_total_sensor_ = s; }

  void set_pv1_voltage_sensor(sensor::Sensor *s) { this->pv_voltage_sensor_[0] = s; }
  void set_pv1_current_sensor(sensor::Sensor *s) { this->pv_current_sensor_[0] = s; }
  void set_pv1_power_sensor(sensor::Sensor *s) { this->pv_power_sensor_[0] = s; }
  void set_pv2_voltage_sensor(sensor::Sensor *s) { this->pv_voltage_sensor_[1] = s; }
  void set_pv2_current_sensor(sensor::Sensor *s) { this->pv_current_sensor_[1] = s; }
  void set_pv2_power_sensor(sensor::Sensor *s) { this->pv_power_sensor_[1] = s; }
  void set_pv3_voltage_sensor(sensor::Sensor *s) { this->pv_voltage_sensor_[2] = s; }
  void set_pv3_current_sensor(sensor::Sensor *s) { this->pv_current_sensor_[2] = s; }
  void set_pv3_power_sensor(sensor::Sensor *s) { this->pv_power_sensor_[2] = s; }
  void set_pv4_voltage_sensor(sensor::Sensor *s) { this->pv_voltage_sensor_[3] = s; }
  void set_pv4_current_sensor(sensor::Sensor *s) { this->pv_current_sensor_[3] = s; }
  void set_pv4_power_sensor(sensor::Sensor *s) { this->pv_power_sensor_[3] = s; }
#endif

#ifdef USE_TEXT_SENSOR
  void set_inverter_status_text_sensor(text_sensor::TextSensor *s) { this->inverter_status_text_sensor_ = s; }
  void set_event_alarms_text_sensor(text_sensor::TextSensor *s) { this->event_alarms_text_sensor_ = s; }
  void set_event_faults_text_sensor(text_sensor::TextSensor *s) { this->event_faults_text_sensor_ = s; }
#endif

 protected:
  std::string host_;
  uint16_t port_{8899};
  uint8_t modbus_address_{1};
  uint32_t logger_serial_{0};
  uint8_t v5_serial_{0};

  bool connect_and_transact_(const std::vector<uint8_t> &request, std::vector<uint8_t> &response);

  std::vector<uint8_t> wrap_v5_request_(uint8_t frame_type, uint16_t sensor_type, const std::vector<uint8_t> &tail);
  bool extract_v5_payload_(const std::vector<uint8_t> &frame, std::vector<uint8_t> &payload);

  std::vector<uint8_t> build_read_request_(uint16_t start_reg, uint16_t count);
  bool parse_response_(const std::vector<uint8_t> &frame, std::vector<uint8_t> &register_data);
  void handle_live_block_(const std::vector<uint8_t> &regs, uint16_t start_reg, uint16_t count);

  std::vector<uint8_t> build_write_request_(uint16_t reg, uint16_t value);
  bool parse_write_response_(const std::vector<uint8_t> &frame, uint16_t expected_reg, uint16_t expected_value);

  std::vector<uint8_t> build_at_command_request_(const std::string &cmd);
  bool parse_at_response_(const std::vector<uint8_t> &frame, std::string &text_out);

  static uint16_t modbus_crc16_(const uint8_t *data, size_t len);
  static uint8_t v5_checksum_(const uint8_t *data, size_t len);

  static uint16_t get_u16_(const std::vector<uint8_t> &regs, uint16_t reg, uint16_t start_reg);
  static uint32_t get_u32_(const std::vector<uint8_t> &regs, uint16_t reg, uint16_t start_reg);

#ifdef USE_SENSOR
  sensor::Sensor *grid_voltage_sensor_{nullptr};
  sensor::Sensor *grid_current_sensor_{nullptr};
  sensor::Sensor *grid_frequency_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *rated_power_sensor_{nullptr};
  sensor::Sensor *current_power_sensor_{nullptr};
  sensor::Sensor *ac_energy_today_sensor_{nullptr};
  sensor::Sensor *ac_energy_total_sensor_{nullptr};
  sensor::Sensor *pv_voltage_sensor_[4]{nullptr, nullptr, nullptr, nullptr};
  sensor::Sensor *pv_current_sensor_[4]{nullptr, nullptr, nullptr, nullptr};
  sensor::Sensor *pv_power_sensor_[4]{nullptr, nullptr, nullptr, nullptr};
#endif

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *inverter_status_text_sensor_{nullptr};
  text_sensor::TextSensor *event_alarms_text_sensor_{nullptr};
  text_sensor::TextSensor *event_faults_text_sensor_{nullptr};
#endif
};

#ifdef USE_NUMBER
// power_percent: sets the Output Coefficient register (max output power, as a
// percent of Rated Power). Optimistic: publishes the requested value right
// away rather than waiting to read it back on the next poll cycle.
class TSunGen3PowerPercentNumber : public number::Number, public Parented<TSunGen3Component> {
 protected:
  void control(float value) override {
    this->parent_->set_power_percent(value);
    this->publish_state(value);
  }
};
#endif

#ifdef USE_OUTPUT
// Same underlying write as the number above, exposed as a plain FloatOutput
// (0.0-1.0) for use as a write_action target from other components/automations
// (e.g. this author's `output_combined`).
class TSunGen3PowerPercentOutput : public output::FloatOutput, public Parented<TSunGen3Component> {
 protected:
  void write_state(float state) override { this->parent_->set_power_percent(state * 100.0f); }
};
#endif

#ifdef USE_BUTTON
class TSunGen3ResetButton : public button::Button, public Parented<TSunGen3Component> {
 protected:
  void press_action() override { this->parent_->send_reset_command(); }
};
#endif

}  // namespace tsungen3
}  // namespace esphome
