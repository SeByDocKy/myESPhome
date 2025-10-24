#pragma once
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace jsy1039 {

class JSY1039 : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_pos_energy_sensor(sensor::Sensor *pos_energy_sensor) { pos_energy_sensor_ = pos_energy_sensor; }
  void set_neg_energy_sensor(sensor::Sensor *neg_energy_sensor) { neg_energy_sensor_ = neg_energy_sensor; }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { frequency_sensor_ = frequency_sensor; }
  void set_power_factor_sensor(sensor::Sensor *power_factor_sensor) { power_factor_sensor_ = power_factor_sensor; }
    
  void setup() override;  
  void update() override;
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  void dump_config() override;
  void reset_energy();
  void read_register04();
  void write_register04(uint8_t new_address , uint8_t new_baudrate);
  uint8_t get_address(void) {return current_address_;}
  uint8_t get_baudrate(void) {return current_baudrate_;}
    
 protected:
 
  uint8_t read_data_ = 1;
  uint8_t new_address_ = 0x01;
  uint8_t new_baudrate_ = 0x06;
  uint8_t current_address_ = 0x01;
  uint8_t current_baudrate_ = 0x06;

  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *pos_energy_sensor_{nullptr};
  sensor::Sensor *neg_energy_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *power_factor_sensor_{nullptr};
  
};

template<typename... Ts> 
class ResetEnergyAction : public Action<Ts...> {
 public:
  ResetEnergyAction(JSY1039 *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->reset_energy(); }
  
 protected:
 JSY1039 *parent_;
};
  
template<typename... Ts> 
class WriteCommunicationSettingAction : public Action<Ts...> {
 public:
  WriteCommunicationSettingAction(JSY1039 *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint8_t, new_address)
  TEMPLATABLE_VALUE(uint8_t, new_baudrate)
  void play(Ts... x) override { this->parent_->write_register04(this->new_address_.value(x...) , this->new_baudrate_.value(x...)); }
  
  protected:
    JSY1039 *parent_;
};

}  // namespace jsy1039
}  // namespace esphome
