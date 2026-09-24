#pragma once

#include "esphome/core/component.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <memory>
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

// Modbus function code used for v1 (read-only telemetry)
static const uint8_t MB_READ_HOLDING_REGISTERS = 0x03;

// Live-data register block (see s-allius/tsun-gen3-proxy wiki: MODBUS registers)
static const uint16_t REG_BLOCK_START = 0x3000;
static const uint16_t REG_BLOCK_COUNT = 0x2A;  // 0x3000 .. 0x3029 inclusive

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

#ifdef USE_SENSOR
  void set_grid_voltage_sensor(sensor::Sensor *s) { this->grid_voltage_sensor_ = s; }
  void set_grid_current_sensor(sensor::Sensor *s) { this->grid_current_sensor_ = s; }
  void set_grid_frequency_sensor(sensor::Sensor *s) { this->grid_frequency_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }
  void set_rated_power_sensor(sensor::Sensor *s) { this->rated_power_sensor_ = s; }
  void set_current_power_sensor(sensor::Sensor *s) { this->current_power_sensor_ = s; }
  void set_ac_daily_energy_sensor(sensor::Sensor *s) { this->ac_daily_energy_sensor_ = s; }
  void set_ac_total_energy_sensor(sensor::Sensor *s) { this->ac_total_energy_sensor_ = s; }

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
  std::vector<uint8_t> build_read_request_(uint16_t start_reg, uint16_t count);
  bool parse_response_(const std::vector<uint8_t> &frame, std::vector<uint8_t> &modbus_payload);
  void handle_live_block_(const std::vector<uint8_t> &regs, uint16_t start_reg, uint16_t count);

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
  sensor::Sensor *ac_daily_energy_sensor_{nullptr};
  sensor::Sensor *ac_total_energy_sensor_{nullptr};
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

}  // namespace tsungen3
}  // namespace esphome
