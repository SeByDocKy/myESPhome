#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <vector>

namespace esphome {
namespace jsy193 {

// template<typename... Ts> class ResetEnergy1Action;
// template<typename... Ts> class ResetEnergy2Action;

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
  
  void reset_energy1_();
  void reset_energy2_();
  void change_modbus_address_(uint8_t new_address);
  void change_modbus_baudrate_(uint8_t new_baudrate);

 protected:
  template<typename... Ts> friend class ResetEnergyAction;
  
  bool read_data_ = false;
  uint8_t new_address_ = 0x01;
  uint8_t new_baudrate_ = 0x06;
 
  uint8_t current_address_ = 0x01;
  uint8_t current_baudrate_ = 0x06;
 
  void set_new_address_(uint8_t new_address);
  void set_new_baudrate_(uint8_t new_baudrate);
  
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

template<typename... Ts> class ResetEnergy1Action : public Action<Ts...> {
 public:
  ResetEnergy1Action(JSY193 *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->reset_energy1_(); }
  
 protected:
 JSY193 *parent_;
};
  
template<typename... Ts> class ResetEnergy2Action : public Action<Ts...> {
 public:
  ResetEnergy2Action(JSY193 *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->reset_energy2_(); }  

 protected:
  JSY193 *parent_;
};

template<typename... Ts> class NewModbusAddressAction : public Action<Ts...> {
 public:
  NewModbusAddressAction(JSY193 *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(int, new_address)

  void play(Ts... x) override { this->parent_->change_modbus_address_(this->new_address_.value(x...)); }

 protected:
  JSY *parent_;
};

template<typename... Ts> class NewModbusBaudRateAction : public Action<Ts...> {
 public:
  NewModbusBaudRateAction(JSY193 *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(int, new_baudrate)

  void play(Ts... x) override { this->parent_->change_modbus_baudrate_(this->new_baudrate_.value(x...)); }

 protected:
  JSY *parent_;
};


}  // namespace jsy193
}  // namespace esphome
