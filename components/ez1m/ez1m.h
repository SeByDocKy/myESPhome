#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/uart/uart.h"
#include <vector>
#include <utility>
#include <string>

namespace esphome {
namespace ez1m {

enum class EZ1MSensorType {
  CH1_DC_VOLTAGE,
  CH2_DC_VOLTAGE,
  CH1_DC_CURRENT,
  CH2_DC_CURRENT,
  CH1_DC_POWER,
  CH2_DC_POWER,
  TOTAL_DC_POWER,
  AC_POWER,
  GRID_FREQUENCY,
  TEMPERATURE,
  DAILY_ENERGY,
  CH1_SESSION_ENERGY,
  CH2_SESSION_ENERGY,
  LIFETIME_ENERGY,
  INVERTER_UPTIME,
};

enum class EZ1MTextSensorType {
  INVERTER_STATE,
  DSP_VERSION,
};

enum class EZ1MNumberType {
  POWER_LIMIT,
  TOTAL_ENERGY,
};

// Forward declarations - concrete headers are only included in ez1m.cpp
class EZ1MSensor;
class EZ1MTextSensor;
class EZ1MNumber;
class EZ1MSwitch;

class EZ1MComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // --- Config setters (codegen) ---
  void set_dc_voltage_divisor(float d) { this->dc_voltage_divisor_ = d; }
  void set_dc_current_divisor(float d) { this->dc_current_divisor_ = d; }
  void set_grid_frequency_divisor(float d) { this->grid_frequency_divisor_ = d; }

  // --- Entity registration (called from each platform's to_code) ---
  void register_sensor(EZ1MSensor *sensor, EZ1MSensorType type);
  void register_text_sensor(EZ1MTextSensor *sensor, EZ1MTextSensorType type);
  void set_power_limit_number(EZ1MNumber *number) { this->power_limit_number_ = number; }
  void set_total_energy_number(EZ1MNumber *number) { this->total_energy_number_ = number; }
  void set_onoff_switch(EZ1MSwitch *sw) { this->onoff_switch_ = sw; }

  // --- Commands (called by EZ1MNumber / EZ1MSwitch) ---
  void set_power_limit(float watts);
  void turn_on(float watts);
  void turn_off();
  float get_power_limit_value() const;

  // --- Lifetime energy (RAM accumulator persisted to flash) ---
  void set_lifetime_energy(float kwh);
  float get_lifetime_energy() const { return this->total_kwh_; }

 protected:
  void publish_sensor_(EZ1MSensorType type, float value);
  void publish_text_sensor_(EZ1MTextSensorType type, const std::string &value);
  void send_poll_request_();
  void handle_frame_(const uint8_t *bytes, size_t frame_len);
  uint16_t watts_to_raw_(float watts) const;
  void save_lifetime_energy_();
  void load_lifetime_energy_();

  std::vector<uint8_t> rx_buffer_;
  std::vector<std::pair<EZ1MSensorType, EZ1MSensor *>> sensors_;
  std::vector<std::pair<EZ1MTextSensorType, EZ1MTextSensor *>> text_sensors_;
  EZ1MNumber *power_limit_number_{nullptr};
  EZ1MNumber *total_energy_number_{nullptr};
  EZ1MSwitch *onoff_switch_{nullptr};

  float dc_voltage_divisor_{50.0f};
  float dc_current_divisor_{88.0f};
  float grid_frequency_divisor_{27.32f};

  float total_kwh_{0.0f};
  float prev_daily_{-1.0f};
  ESPPreferenceObject pref_;
};

}  // namespace ez1m
}  // namespace esphome
