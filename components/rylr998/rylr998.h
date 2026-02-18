#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"

// Forward declarations — évitent d'inclure les headers dans ce fichier
namespace esphome { namespace sensor { class Sensor; } }
namespace esphome { namespace number  { class Number;  } }

namespace esphome {
namespace rylr998 {

class RYLR998Component : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_address(uint16_t address) { this->address_ = address; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_spreading_factor(uint8_t sf) { this->spreading_factor_ = sf; }
  void set_bandwidth(uint32_t bw) { this->bandwidth_ = bw; }
  void set_coding_rate(uint8_t cr) { this->coding_rate_ = cr; }
  void set_preamble_length(uint8_t pl) { this->preamble_length_ = pl; }
  void set_network_id(uint8_t nid) { this->network_id_ = nid; }
  void set_tx_power(uint8_t tp) { this->tx_power_ = tp; }

  // ── Sensor RSSI / SNR (optionnels) ────────────────────────────────────────
  void set_rssi_sensor(esphome::sensor::Sensor *s) { this->rssi_sensor_ = s; }
  void set_snr_sensor(esphome::sensor::Sensor *s)  { this->snr_sensor_  = s; }

  // ── Number TX power (optionnel) ───────────────────────────────────────────
  // Envoie AT+CRFOP=<power> et met à jour tx_power_ à chaud.
  // Appelé par RYLR998TxPowerNumber::control().
  void apply_tx_power(uint8_t power);
  uint8_t get_tx_power(){ return this->tx_power_ ;}
  void set_tx_power_number(esphome::number::Number *n) { this->tx_power_number_ = n; }

  bool transmit_packet(const std::vector<uint8_t> &data);
  bool transmit_packet(uint16_t destination, const std::vector<uint8_t> &data);

  bool send_data(uint16_t destination, const std::vector<uint8_t> &data);
  bool send_data(uint16_t destination, const std::string &data);

  // Plain C function pointer callback - NO virtual dispatch, NO vtable, NO heap
  // Completely immune to heap corruption from PacketTransport::setup()
  typedef void (*packet_raw_cb_t)(const std::vector<uint8_t> &data, float rssi, float snr);
  void set_raw_packet_callback(packet_raw_cb_t cb) { this->raw_cb_ = cb; }

  void set_packet_trigger(Trigger<std::vector<uint8_t>, float, float> *trigger) {
    this->packet_trigger_ = trigger;
  }

  void add_on_packet_callback(std::function<void(uint16_t, std::vector<uint8_t>, int, int)> cb) {
    this->packet_callback_.add(std::move(cb));
  }

 protected:
  uint16_t address_{0};
  uint32_t frequency_{915000000};
  uint8_t spreading_factor_{9};
  uint32_t bandwidth_{125000};
  uint8_t coding_rate_{1};
  uint8_t preamble_length_{12};
  uint8_t network_id_{18};
  uint8_t tx_power_{22};

  bool initialized_{false};
  std::string rx_buffer_;
  uint32_t last_command_time_{0};
  static const uint32_t COMMAND_DELAY_MS = 100;

  // TX rate limiting
  uint32_t last_tx_time_{0};
  static const uint32_t TX_MIN_INTERVAL_MS = 600;

  bool send_command_(const std::string &command, uint32_t timeout_ms = 1000);
  void send_raw_(const std::string &command);
  void process_rx_line_(const std::string &line);
  uint8_t bandwidth_to_code_(uint32_t bandwidth);
  std::string bandwidth_to_string_(uint32_t bandwidth);

  // Plain function pointer - NO vtable, NO virtual, NO heap allocation
  packet_raw_cb_t raw_cb_{nullptr};

  // ── Sensors optionnels RSSI / SNR ─────────────────────────────────────────
  esphome::sensor::Sensor *rssi_sensor_{nullptr};
  esphome::sensor::Sensor *snr_sensor_{nullptr};

  // ── Number optionnel TX power ──────────────────────────────────────────────
  esphome::number::Number *tx_power_number_{nullptr};

  Trigger<std::vector<uint8_t>, float, float> *packet_trigger_{nullptr};
  CallbackManager<void(uint16_t, std::vector<uint8_t>, int, int)> packet_callback_;
};

}  // namespace rylr998
}  // namespace esphome
