#pragma once

// rylr998.h — mise à jour pour cohérence avec rylr998.cpp corrigé
// Changements par rapport à l'original :
//   [H2]   uint16_t lora_air_time_  →  uint32_t lora_air_time_
//   [M3]   send_raw_(const std::string &)  →  send_raw_(const char *)
//   [minor] bandwidth_to_string_() : std::string  →  const char *
//   [H3]   NOTE : packet_trigger_ devrait idéalement être un
//           std::vector<Trigger*> et set_packet_trigger() remplacée par
//           add_packet_trigger() pour supporter plusieurs on_packet: en YAML.
//           Ce changement nécessite aussi une mise à jour de __init__.py.

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"

namespace esphome { namespace sensor { class Sensor; } }
namespace esphome { namespace number  { class Number;  } }

namespace esphome {
namespace rylr998 {

// [VARIANT] Les modules RYLR998 et RYLR999 partagent le même jeu de commandes
//           AT côté port LoRa (AT+ADDRESS, AT+BAND, AT+PARAMETER, AT+NETWORKID,
//           AT+CRFOP, AT+SEND, +RCV — vérifié par comparaison des docs REYAX).
//           La seule différence fonctionnelle notée est la puissance RF max :
//           22dBm (RYLR998, SX1262) vs 30dBm (RYLR999). `variant_` sert de
//           garde-fou runtime pour ne jamais envoyer AT+CRFOP au-delà de ce
//           que le PA du module cible supporte, même si tx_power est modifié
//           dynamiquement via l'entité number ou une action rylr998.send_packet.
enum class RYLRVariant : uint8_t {
  RYLR998 = 0,
  RYLR999 = 1,
};

class RYLR998Component : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_address(uint16_t address)         { this->address_         = address; }
  void set_frequency(uint32_t frequency)     { this->frequency_       = frequency; }
  void set_spreading_factor(uint8_t sf)      { this->spreading_factor_= sf; }
  void set_bandwidth(uint32_t bw)            { this->bandwidth_       = bw; }
  void set_coding_rate(uint8_t cr)           { this->coding_rate_     = cr; }
  void set_preamble_length(uint8_t pl)       { this->preamble_length_ = pl; }
  void set_network_id(uint8_t nid)           { this->network_id_      = nid; }
  void set_tx_power(uint8_t tp)              { this->tx_power_        = tp; }
  void set_air_time(bool enable)             { this->compute_air_time_= enable; }
  void set_variant(RYLRVariant variant)      { this->variant_         = variant; }
  RYLRVariant get_variant() const            { return this->variant_; }

  void set_rssi_sensor(esphome::sensor::Sensor *s)       { this->rssi_sensor_       = s; }
  void set_snr_sensor(esphome::sensor::Sensor *s)        { this->snr_sensor_        = s; }
  void set_last_error_sensor(esphome::sensor::Sensor *s) { this->last_error_sensor_ = s; }

  void apply_tx_power(uint8_t power);
  uint8_t get_tx_power()                     { return this->tx_power_; }
  void set_tx_power_number(esphome::number::Number *n) { this->tx_power_number_ = n; }

  bool transmit_packet(const std::vector<uint8_t> &data);
  bool transmit_packet(uint16_t destination, const std::vector<uint8_t> &data);

  bool send_data(uint16_t destination, const std::vector<uint8_t> &data);
  bool send_data(uint16_t destination, const std::string &data);

  typedef void (*packet_raw_cb_t)(const std::vector<uint8_t> &data, float rssi, float snr);
  void set_raw_packet_callback(packet_raw_cb_t cb) { this->raw_cb_ = cb; }

  void set_packet_trigger(Trigger<std::vector<uint8_t>, float, float> *trigger) {
    this->packet_trigger_ = trigger;
    // [H3] NOTE : un seul trigger est stocké — si plusieurs on_packet: sont
    //      déclarés en YAML, seul le dernier est actif (silencieux).
    //      Correction complète : remplacer packet_trigger_ par un
    //      std::vector<Trigger*> et adapter __init__.py en conséquence.
  }

  void add_on_packet_callback(std::function<void(uint16_t, std::vector<uint8_t>, int, int)> cb) {
    this->packet_callback_.add(std::move(cb));
  }

 protected:
  uint16_t address_{0};
  uint32_t frequency_{915000000};
  uint8_t  spreading_factor_{9};
  uint32_t bandwidth_{125000};
  uint8_t  coding_rate_{1};
  uint8_t  preamble_length_{12};
  uint8_t  network_id_{18};
  uint8_t  tx_power_{22};

  // [H2] CORRECTION : uint16_t → uint32_t.
  //      L'ancienne valeur débordait silencieusement pour les SF élevés
  //      (ex. SF12/BW125 → airtime ~5 000 ms rentre dans uint16_t,
  //       mais avec le bug H1 bw_code=7 le résultat calc dépassait 22 millions ms).
  uint32_t lora_air_time_{600};
  bool     compute_air_time_{false};

  bool        initialized_{false};
  std::string rx_buffer_;
  uint32_t    last_command_time_{0};
  uint32_t    last_tx_time_{0};

  bool        send_command_(const std::string &command, uint32_t timeout_ms = 1000);

  // [M3] CORRECTION : const char * au lieu de const std::string &
  //      pour éviter l'alloc heap à chaque TX.
  void        send_raw_(const char *command);

  void        process_rx_line_(const std::string &line);
  uint8_t     bandwidth_to_code_(uint32_t bandwidth);

  // [minor] const char * au lieu de std::string (pas d'alloc heap dans dump_config)
  const char *bandwidth_to_string_(uint32_t bandwidth);

  // [VARIANT] Puissance RF max supportée par le PA du module (22dBm RYLR998 / 30dBm RYLR999).
  uint8_t     max_tx_power_() const {
    return this->variant_ == RYLRVariant::RYLR999 ? 30 : 22;
  }
  const char *variant_name_() const {
    return this->variant_ == RYLRVariant::RYLR999 ? "RYLR999" : "RYLR998";
  }

  RYLRVariant variant_{RYLRVariant::RYLR998};

  packet_raw_cb_t raw_cb_{nullptr};

  esphome::sensor::Sensor *rssi_sensor_{nullptr};
  esphome::sensor::Sensor *snr_sensor_{nullptr};
  esphome::sensor::Sensor *last_error_sensor_{nullptr};
  esphome::number::Number *tx_power_number_{nullptr};

  Trigger<std::vector<uint8_t>, float, float> *packet_trigger_{nullptr};
  CallbackManager<void(uint16_t, std::vector<uint8_t>, int, int)> packet_callback_;
};

}  // namespace rylr998
}  // namespace esphome
