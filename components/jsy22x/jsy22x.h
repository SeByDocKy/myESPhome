#pragma once
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace jsy22x {

class JSY22X : public PollingComponent, public modbus::ModbusDevice {
 public:
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_active_power_sensor(sensor::Sensor *active_power_sensor) { active_power_sensor_ = active_power_sensor; }
  void set_active_energy_sensor(sensor::Sensor *active_energy_sensor) { active_energy_sensor_ = active_energy_sensor; }
  void set_power_factor_sensor(sensor::Sensor *power_factor_sensor) { power_factor_sensor_ = power_factor_sensor; }
  void set_reactive_power_sensor(sensor::Sensor *reactive_power_sensor) { reactive_power_sensor_ = reactive_power_sensor; }
  void set_reactive_energy_sensor(sensor::Sensor *reactive_energy_sensor) { reactive_energy_sensor_ = reactive_energy_sensor; }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { frequency_sensor_ = frequency_sensor; }
  void set_acdc_mode_sensor(sensor::Sensor *acdc_mode_sensor) { acdc_mode_sensor_ = acdc_mode_sensor; }
   
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
  sensor::Sensor *active_power_sensor_{nullptr};
  sensor::Sensor *active_energy_sensor_{nullptr};
  sensor::Sensor *power_factor_sensor_{nullptr};
  sensor::Sensor *reactive_power_sensor_{nullptr};
  sensor::Sensor *reactive_energy_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *acdc_mode_sensor_{nullptr};
  
};

template<typename... Ts> 
class ResetEnergyAction : public Action<Ts...> {
 public:
  ResetEnergyAction(JSY22X *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->reset_energy(); }
  
 protected:
 JSY22X *parent_;
};
  

template<typename... Ts> 
class WriteCommunicationSettingAction : public Action<Ts...> {
 public:
  WriteCommunicationSettingAction(JSY22X *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint8_t, new_address)
  TEMPLATABLE_VALUE(uint8_t, new_baudrate)
  void play(Ts... x) override { this->parent_->write_register04(this->new_address_.value(x...) , this->new_baudrate_.value(x...)); }
  
  protected:
    JSY22X *parent_;
};

}  // namespace jsy22x
}  // namespace esphome
