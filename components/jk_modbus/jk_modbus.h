#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include <string>

namespace esphome::jk_modbus {

// Hex dump used by the log statements ("4E.57.00..."). ESPHome's own format_hex_pretty() changed
// signature several times (the std::string overload is gone in recent nightlies), so keep a tiny
// local helper instead of depending on it. Only evaluated when the log level is enabled.
inline std::string hex_dump(const uint8_t *data, size_t len) {
  static const char HEX_DIGITS[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(len * 3);
  for (size_t i = 0; i < len; i++) {
    if (i > 0)
      out += '.';
    out += HEX_DIGITS[data[i] >> 4];
    out += HEX_DIGITS[data[i] & 0x0F];
  }
  return out;
}

class JkModbusDevice;

class JkModbus : public uart::UARTDevice, public Component {
 public:
  JkModbus() = default;

  void setup() override;

  void loop() override;

  void dump_config() override;

  void register_device(JkModbusDevice *device) { this->devices_.push_back(device); }

  float get_setup_priority() const override;

  void send(uint8_t function, uint8_t address, uint8_t value);
  void write_register(uint8_t address, uint8_t value);
  void read_registers();
  void set_rx_timeout(uint16_t rx_timeout) { rx_timeout_ = rx_timeout; }

  void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }

 protected:
  GPIOPin *flow_control_pin_{nullptr};

  void authenticate_();
  bool parse_jk_modbus_byte_(uint8_t byte);

  std::vector<uint8_t> rx_buffer_;
  std::vector<uint8_t> frame_data_;
  uint16_t rx_timeout_{50};
  uint32_t last_jk_modbus_byte_{0};
  uint32_t write_busy_until_{0};
  bool write_busy_{false};
  std::vector<JkModbusDevice *> devices_;
};

class JkModbusDevice {
 public:
  void set_parent(JkModbus *parent) { parent_ = parent; }
  void set_address(uint8_t address) { address_ = address; }
  virtual void on_jk_modbus_data(const uint8_t &function, const std::vector<uint8_t> &data) = 0;

  void send(int8_t function, uint8_t address, uint8_t value) { this->parent_->send(function, address, value); }
  void write_register(uint8_t address, uint8_t value) { this->parent_->write_register(address, value); }
  void read_registers() { this->parent_->read_registers(); }

 protected:
  friend JkModbus;

  JkModbus *parent_;
  uint8_t address_;
};

}  // namespace esphome::jk_modbus
