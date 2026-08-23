#pragma once

#include "esphome/core/component.h"
#include "esphome/components/async_tcp/async_tcp.h"

#include <cstring>
#include <memory>
#include <vector>
#include <queue>

namespace esphome::modbustcp {

class ModbusDevice;

class ModbusTCP :  public Component {

 public:
 
   ModbusTCP() = default;

  void setup() override;

  void loop() override;

  void dump_config() override;

  void register_device(ModbusDevice *device) { this->devices_.push_back(device); }

  float get_setup_priority() const override;

  void send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
            uint8_t payload_len = 0, const uint8_t *payload = nullptr);
  void send_raw(const std::vector<uint8_t> &payload);
  uint8_t waiting_for_response{0};
  void set_send_wait_time(uint16_t time_in_ms) { send_wait_time_ = time_in_ms; }
  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  
  // Nouvelle méthode pour changer d'hôte à la volée et se reconnecter
  void set_host_and_reconnect(const std::string &host);

  void handle_message(uint8_t byte[256]);
  void on_shutdown() override;
  void connect();
  //void send_message(const std::string& message);
  void send_message(const uint8_t *send_byte);
  AsyncClient* get_client() const { return this->client_; }

 protected:
 
  AsyncClient* client_{nullptr};
  bool connected_{false};
  //bool parse_modbus_byte_(uint8_t byte);
  uint16_t send_wait_time_{250};
  uint32_t last_modbus_byte_{0};
  uint32_t last_send_{0};
  std::vector<ModbusDevice *> devices_;
  uint16_t Transaction_Identifier = 0;
  uint32_t last_attempt_{0};
  uint16_t port_;
  std::string host_;
  
};

class ModbusDevice {
 public:
  void set_parent(ModbusTCP *parent) { parent_ = parent; }
  void set_address(uint8_t address) { address_ = address; }
  virtual void on_modbus_data(const std::vector<uint8_t> &data) = 0;
  virtual void on_modbus_error(uint8_t function_code, uint8_t exception_code) {}
  virtual void on_modbus_read_registers(uint8_t function_code, uint16_t start_address, uint16_t number_of_registers){};
  virtual void on_modbus_write_registers(uint8_t function_code, const std::vector<uint8_t> &data){};
  void send(uint8_t function, uint16_t start_address, uint16_t number_of_entities, uint8_t payload_len = 0,
            const uint8_t *payload = nullptr) {
    this->parent_->send(this->address_, function, start_address, number_of_entities, payload_len, payload);
  }
  void send_raw(const std::vector<uint8_t> &payload) { this->parent_->send_raw(payload); }
  void send_error(uint8_t function_code, uint8_t exception_code) {
    std::vector<uint8_t> error_response;
    error_response.reserve(3);
    error_response.push_back(this->address_);
    error_response.push_back(function_code | 0x80);
    error_response.push_back(exception_code);
    this->send_raw(error_response);
  }
  // If more than one device is connected block sending a new command before a response is received
  bool waiting_for_response() { return parent_->waiting_for_response != 0; }

 protected:
  friend ModbusTCP;
  
  ModbusTCP *parent_;
  uint8_t address_;
};

}  // namespace esphome::modbustcp
