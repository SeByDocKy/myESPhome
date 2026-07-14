#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace st_meter {

class STMeter : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_voltage_sensor(uint8_t phase, sensor::Sensor *voltage_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].voltage_sensor_ = voltage_sensor;
  }
  void set_current_sensor(uint8_t phase, sensor::Sensor *current_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].current_sensor_ = current_sensor;
  }
  void set_active_power_sensor(uint8_t phase, sensor::Sensor *active_power_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].active_power_sensor_ = active_power_sensor;
  }
  void set_reactive_power_sensor(uint8_t phase, sensor::Sensor *reactive_power_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].reactive_power_sensor_ = reactive_power_sensor;
  }
  void set_power_factor_sensor(uint8_t phase, sensor::Sensor *power_factor_sensor) {
    this->phases_[phase].setup = true;
    this->phases_[phase].power_factor_sensor_ = power_factor_sensor;
  }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { this->frequency_sensor_ = frequency_sensor; }
  void set_total_active_power_sensor(sensor::Sensor *total_active_power_sensor) {
    this->total_active_power_sensor_ = total_active_power_sensor;
  }
  void set_total_reactive_power_sensor(sensor::Sensor *total_reactive_power_sensor) {
    this->total_reactive_power_sensor_ = total_reactive_power_sensor;
  }
  void set_total_active_electricity_power_sensor(sensor::Sensor *total_active_electricity_power_sensor) {
    this->total_active_electricity_power_sensor_ = total_active_electricity_power_sensor;
  }
  void set_total_reactive_electricity_power_sensor(sensor::Sensor *total_reactive_electricity_power_sensor) {
    this->total_reactive_electricity_power_sensor_ = total_reactive_electricity_power_sensor;
  }

  void update() override;

  void on_modbus_data(const std::vector<uint8_t> &data) override;

  void dump_config() override;

 protected:
  struct STPhase {
    bool setup{false};
    sensor::Sensor *voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *active_power_sensor_{nullptr};
    sensor::Sensor *reactive_power_sensor_{nullptr};
    sensor::Sensor *power_factor_sensor_{nullptr};
  } phases_[3];
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *total_active_power_sensor_{nullptr};
  sensor::Sensor *total_active_electricity_power_sensor_{nullptr};
  sensor::Sensor *total_reactive_power_sensor_{nullptr};
  sensor::Sensor *total_reactive_electricity_power_sensor_{nullptr};
};

}  // namespace st_meter
}  // namespace esphome
