#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace jsy193 {

template<typename... Ts> class ResetEnergyAction;

class JSY193 : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_voltage1_sensor(sensor::Sensor *voltage1_sensor) { voltage1_sensor_ = voltage1_sensor; }
  void set_current1_sensor(sensor::Sensor *current1_sensor) { current1_sensor_ = current1_sensor; }
  void set_power1_sensor(sensor::Sensor *power1_sensor) { power1_sensor_ = power1_sensor; }
  void set_pos_energy1_sensor(sensor::Sensor *pos_energy1_sensor) { pos_energy1_sensor_ = pos_energy1_sensor; }
  void set_neg_energy1_sensor(sensor::Sensor *neg_energy1_sensor) { neg_energy1_sensor_ = neg_energy1_sensor; }
  void set_frequency1_sensor(sensor::Sensor *frequency1_sensor) { frequency1_sensor_ = frequency1_sensor; }
  void set_power_factor1_sensor(sensor::Sensor *power_factor1_sensor) { power_factor1_sensor_ = power_factor1_sensor; }
  
  void set_voltage2_sensor(sensor::Sensor *voltage2_sensor) { voltage2_sensor_ = voltage2_sensor; }
  void set_current2_sensor(sensor::Sensor *current2_sensor) { current2_sensor_ = current2_sensor; }
  void set_power2_sensor(sensor::Sensor *power2_sensor) { power2_sensor_ = power2_sensor; }
  void set_pos_energy2_sensor(sensor::Sensor *pos_energy2_sensor) { pos_energy2_sensor_ = pos_energy2_sensor; }
  void set_neg_energy2_sensor(sensor::Sensor *neg_energy2_sensor) { neg_energy2_sensor_ = neg_energy2_sensor; }
  void set_frequency2_sensor(sensor::Sensor *frequency2_sensor) { frequency2_sensor_ = frequency2_sensor; }
  void set_power_factor2_sensor(sensor::Sensor *power_factor2_sensor) { power_factor2_sensor_ = power_factor2_sensor; }


  void update() override;

  void on_modbus_data(const std::vector<uint8_t> &data) override;

  void dump_config() override;

 protected:
  template<typename... Ts> friend class ResetEnergyAction;
  sensor::Sensor *voltage1_sensor_{nullptr};
  sensor::Sensor *current1_sensor_{nullptr};
  sensor::Sensor *power1_sensor_{nullptr};
  sensor::Sensor *pos_energy1_sensor_{nullptr};
  sensor::Sensor *neg_energy1_sensor_{nullptr};
  sensor::Sensor *frequency1_sensor_{nullptr};
  sensor::Sensor *power_factor1_sensor_{nullptr};
  
  sensor::Sensor *voltage2_sensor_{nullptr};
  sensor::Sensor *current2_sensor_{nullptr};
  sensor::Sensor *power2_sensor_{nullptr};
  sensor::Sensor *pos_energy2_sensor_{nullptr};
  sensor::Sensor *neg_energy2_sensor_{nullptr};
  sensor::Sensor *frequency2_sensor_{nullptr};
  sensor::Sensor *power_factor2_sensor_{nullptr};

  void reset_energy_();
};

template<typename... Ts> class ResetEnergyAction : public Action<Ts...> {
 public:
  ResetEnergyAction(JSY193 *jsy193) : jsy193_(jsy193) {}

  void play(Ts... x) override { this->jsy193_->reset_energy_(); }

 protected:
  JSY193 *jsy193_;
};

}  // namespace jsy193
}  // namespace esphome
