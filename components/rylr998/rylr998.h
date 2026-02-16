#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace rylr998 {

// Forward declaration
class RYLR998Listener;

class RYLR998Component : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration setters
  void set_address(uint16_t address) { this->address_ = address; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_spreading_factor(uint8_t spreading_factor) { this->spreading_factor_ = spreading_factor; }
  void set_bandwidth(uint32_t bandwidth) { this->bandwidth_ = bandwidth; }
  void set_coding_rate(uint8_t coding_rate) { this->coding_rate_ = coding_rate; }
  void set_preamble_length(uint8_t preamble_length) { this->preamble_length_ = preamble_length; }
  void set_network_id(uint8_t network_id) { this->network_id_ = network_id; }
  void set_tx_power(uint8_t tx_power) { this->tx_power_ = tx_power; }

  // Transmit packet (called by listeners like RYLR998Transport)
  bool transmit_packet(const std::vector<uint8_t> &data);
  bool transmit_packet(uint16_t destination, const std::vector<uint8_t> &data);

  // Send data (legacy API)
  bool send_data(uint16_t destination, const std::vector<uint8_t> &data);
  bool send_data(uint16_t destination, const std::string &data);

  // Register listener for packet reception
  void register_listener(RYLR998Listener *listener) { this->listeners_.push_back(listener); }

  // Get trigger for automation
  Trigger<std::vector<uint8_t>, float, float> *get_packet_trigger() { return &this->packet_trigger_; }

  // Legacy callback support (for backward compatibility)
  void add_on_packet_callback(std::function<void(uint16_t, std::vector<uint8_t>, int, int)> callback) {
    this->packet_callback_.add(std::move(callback));
  }

 protected:
  // Configuration parameters
  uint16_t address_{0};
  uint32_t frequency_{915000000};  // Default for RYLR998
  uint8_t spreading_factor_{9};
  uint32_t bandwidth_{125000};  // 125 KHz
  uint8_t coding_rate_{1};      // 4/5
  uint8_t preamble_length_{12};
  uint8_t network_id_{18};
  uint8_t tx_power_{22};        // 22 dBm

  // State
  bool initialized_{false};
  std::string rx_buffer_;
  uint32_t last_command_time_{0};
  static const uint32_t COMMAND_DELAY_MS = 100;

  // Helper methods
  bool send_command_(const std::string &command, uint32_t timeout_ms = 1000);
  void process_rx_line_(const std::string &line);
  uint8_t bandwidth_to_code_(uint32_t bandwidth);
  std::string bandwidth_to_string_(uint32_t bandwidth);

  // Listeners and callbacks
  std::vector<RYLR998Listener *> listeners_;
  Trigger<std::vector<uint8_t>, float, float> packet_trigger_;
  
  // Legacy callback support
  CallbackManager<void(uint16_t, std::vector<uint8_t>, int, int)> packet_callback_;
};

// Listener interface for components that want to receive packets
class RYLR998Listener {
 public:
  virtual void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) = 0;
};

}  // namespace rylr998
}  // namespace esphome
