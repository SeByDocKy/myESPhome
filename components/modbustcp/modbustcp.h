#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/async_tcp/async_tcp.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace esphome::modbustcp {

class ModbusDevice;

// Modbus/TCP ADU = MBAP header (7 bytes) + PDU:
//   [0..1] transaction id  [2..3] protocol id (always 0)  [4..5] length (unit id + PDU)  [6] unit id  [7..] PDU
static constexpr size_t MBAP_HEADER_SIZE = 7;
static constexpr size_t MAX_ADU_SIZE = 260;
// Safety cap for the receive buffers: a reply is never larger than MAX_ADU_SIZE.
static constexpr size_t MAX_RX_BUFFER = 1024;

/** Modbus/TCP client (one TCP connection to one host).
 *
 *  Threading: the AsyncClient callbacks run on the async_tcp task, not on the ESPHome main loop. They only
 *  append raw bytes to `rx_buffer_` (guarded by `rx_mutex_`) and flip `connected_`. All frame parsing and every
 *  call into the registered devices happens in loop() on the main task, so the devices never need locking.
 *
 *  One request is in flight at a time (`waiting_for_response`). A reply is matched to the request by
 *  transaction id, so a late reply to an earlier (timed out) attempt is ignored instead of being paired with a
 *  newer request.
 */
class ModbusTCP : public Component {
 public:
  ModbusTCP() = default;

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void on_shutdown() override;

  void register_device(ModbusDevice *device) { this->devices_.push_back(device); }

  /// Build and send a request. `payload` carries the value(s) for the write function codes
  /// (0x05/0x06: 2 bytes, 0x0F/0x10: `payload_len` bytes).
  void send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities,
            uint8_t payload_len = 0, const uint8_t *payload = nullptr);
  /// `payload` = [unit id][function code][data...] (no CRC, no MBAP header: it is added here).
  void send_raw(const std::vector<uint8_t> &payload);

  /// Non zero while a request is in flight (the unit id it was sent to). Cleared by the reply or by the timeout.
  uint8_t waiting_for_response{0};

  /// Time to wait for a reply before the request is considered lost (the device then retries it).
  void set_send_wait_time(uint16_t time_in_ms) { this->send_wait_time_ = time_in_ms; }
  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  /// Change the target host on the fly and reconnect.
  void set_host_and_reconnect(const std::string &host);

  void connect();
  bool is_connected() const { return this->connected_; }
  AsyncClient *get_client() const { return this->client_; }

 protected:
  // async_tcp task context
  void on_rx_(const uint8_t *data, size_t len);

  // main loop context
  void process_rx_();
  void handle_frame_(const uint8_t *frame, size_t len);
  void send_pdu_(uint8_t unit, const uint8_t *pdu, size_t pdu_len);
  bool write_frame_(const uint8_t *frame, size_t len);
  void reset_link_state_();

  AsyncClient *client_{nullptr};
  std::atomic<bool> connected_{false};
  std::atomic<bool> reset_requested_{false};  // set from the async task when a connection (re)opens

  uint16_t send_wait_time_{250};
  uint32_t last_send_{0};
  uint32_t last_attempt_{0};
  uint16_t port_{502};
  std::string host_;

  // Request currently in flight (main loop only)
  bool awaiting_response_{false};
  uint16_t transaction_id_{0};
  uint16_t pending_tid_{0};
  uint8_t pending_unit_{0};
  uint8_t pending_function_{0};

  // Bytes received by the async task, waiting to be moved to `pending_rx_`
  Mutex rx_mutex_;
  std::vector<uint8_t> rx_buffer_;
  // Partially received frames (main loop only)
  std::vector<uint8_t> pending_rx_;

  std::vector<ModbusDevice *> devices_;
};

class ModbusDevice {
 public:
  virtual ~ModbusDevice() = default;
  void set_parent(ModbusTCP *parent) { this->parent_ = parent; }
  void set_address(uint8_t address) { this->address_ = address; }
  /// Reply payload: for reads (FC 1-4) the data bytes after the byte count; for FC 5/6/15/16 the 4 echoed
  /// bytes (address + value/quantity).
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
  bool waiting_for_response() { return this->parent_->waiting_for_response != 0; }

 protected:
  friend ModbusTCP;

  ModbusTCP *parent_{nullptr};
  uint8_t address_{0};
};

}  // namespace esphome::modbustcp
