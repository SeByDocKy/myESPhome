#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace jsy193 {

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
  
  void setup() override;  
  void update() override;
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  void dump_config() override;
  void reset_energy1();
  void reset_energy2();
  void change_address(uint8_t new_address);
  void change_baudrate(uint8_t new_baudrate);
  uint8_t get_address(void) {return current_address_;}
  uint8_t get_baudrate(void) {return current_baudrate_;}
    
 protected:

  uint8_t new_address_;
  uint8_t new_baudrate_;
  bool read_data_ = false;
  uint8_t current_address_ = 0x01;
  uint8_t current_baudrate_ = 0x06;

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
};

template<typename... Ts> 
class ResetEnergy1Action : public Action<Ts...> {
 public:
  ResetEnergy1Action(JSY193 *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->reset_energy1(); }
  
 protected:
 JSY193 *parent_;
};
  
template<typename... Ts> 
class ResetEnergy2Action : public Action<Ts...> {
 public:
  ResetEnergy2Action(JSY193 *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->reset_energy2(); }  

 protected:
  JSY193 *parent_;
};

template<typename... Ts> 
class ChangeAddressAction : public Action<Ts...> {
 public:
  ChangeAddressAction(JSY193 *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint8_t, new_address)  
  void play(Ts... x) override { this->parent_->change_address(this->new_address_.value(x...)); }
  
  protected:
    JSY193 *parent_;
};

template<typename... Ts> 
class ChangeBaudRateAction : public Action<Ts...> {
 public:
  ChangeBaudRateAction(JSY193 *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(uint8_t, new_baudrate)
  void play(Ts... x) override { this->parent_->change_baudrate(this->new_baudrate_.value(x...)); }
  
  protected:
    JSY193 *parent_;
};

}  // namespace jsy193
}  // namespace esphome
