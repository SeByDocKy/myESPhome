#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifdef USE_ESP32
#include <esp_gattc_api.h>
#include <string>
#include <vector>

namespace esphome::rylr999_ble {

namespace espbt = esphome::esp32_ble_tracker;

// Timing de la machine à état de configuration LoRa (tout est asynchrone en
// BLE : pas de lecture bloquante comme sur UART, on attend un "+OK" en
// notify après chaque commande, avec retry sur timeout).
static constexpr uint32_t SETUP_STEP_TIMEOUT_MS = 2000;
static constexpr uint8_t SETUP_MAX_RETRIES = 5;

// Espace utile pour un payload brut (avant encodage hex) : le RYLR999 limite
// AT+SEND à 240 octets ASCII, on encode nos données binaires en hex (2
// caractères / octet), et on garde de la marge pour rester sous le MTU BLE
// négocié (ESPHome ne l'expose pas publiquement depuis esp32_ble_client,
// donc cette constante est une borne conservatrice plutôt qu'une valeur
// dynamique - à ajuster si vous constatez des troncatures en usage réel).
static constexpr size_t MAX_RAW_PACKET_SIZE = 80;

// Interface observateur : la plateforme packet_transport::rylr999_ble
// s'enregistre ici pour recevoir les paquets décodés depuis les trames
// "+RCV=..." reçues en BLE. Même principe que sx126x::SX126xListener.
class RYLR999BLEListener {
 public:
  virtual ~RYLR999BLEListener() = default;
  virtual void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) = 0;
};

enum class SetupStep : uint8_t {
  DISCOVER = 0,  // en attente de la découverte des services / abonnement notify
  ADDRESS,
  BAND,
  PARAMETER,
  NETWORKID,
  CRFOP,
  READY,
  FAILED,
};

class RYLR999BLEComponent : public Component, public ble_client::BLEClientNode {
 public:
  void loop() override;
  void dump_config() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  // UUID service / caractéristique (une seule caractéristique NOTIFY + WRITE
  // NO RESPONSE, conforme au mode transparent BLE du RYLR999).
  void set_service_uuid16(uint16_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_service_uuid32(uint32_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_service_uuid128(uint8_t *uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
  void set_char_uuid16(uint16_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_char_uuid32(uint32_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_char_uuid128(uint8_t *uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }
  // Caractéristique de notification : distincte de l'écriture sur certains
  // modules (cf. rylr999_ble.cpp, découverte en pratique sur le RYLR999).
  void set_notify_char_uuid16(uint16_t uuid) { this->notify_char_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_notify_char_uuid32(uint32_t uuid) { this->notify_char_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_notify_char_uuid128(uint8_t *uuid) { this->notify_char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  // Configuration LoRa (envoyée en BLE au lieu de l'UART utilisé par rylr998).
  void set_address(uint16_t address) { this->address_ = address; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  void set_spreading_factor(uint8_t sf) { this->spreading_factor_ = sf; }
  void set_bandwidth(uint32_t bw) { this->bandwidth_ = bw; }
  void set_coding_rate(uint8_t cr) { this->coding_rate_ = cr; }
  void set_preamble_length(uint8_t pl) { this->preamble_length_ = pl; }
  void set_network_id(uint8_t nid) { this->network_id_ = nid; }
  void set_tx_power(uint8_t tp) { this->tx_power_ = tp; }
  void set_air_time(bool enable) { this->compute_air_time_ = enable; }
  void set_min_tx_interval_ms(uint32_t ms) { this->min_tx_interval_ms_ = ms; }

#ifdef USE_BINARY_SENSOR
  void set_lora_ready_binary_sensor(binary_sensor::BinarySensor *sensor) {
    this->lora_ready_binary_sensor_ = sensor;
  }
#endif

  void register_listener(RYLR999BLEListener *listener) { this->listeners_.push_back(listener); }

  bool is_lora_ready() const { return this->step_ == SetupStep::READY; }
  size_t get_max_packet_size() const { return MAX_RAW_PACKET_SIZE; }

  // Encode le paquet en hex-ASCII et l'envoie via AT+SEND=0,<len>,<hex> (0 =
  // broadcast LoRa ; packet_transport gère lui-même l'identification de
  // l'émetteur/destinataire au niveau applicatif). Retourne false si la
  // configuration LoRa n'est pas encore prête ou si le paquet dépasse
  // get_max_packet_size().
  bool send_payload(const std::vector<uint8_t> &payload);

 protected:
  void advance_to_(SetupStep step);
  void send_current_step_();
  std::string build_step_command_(SetupStep step) const;
  bool send_at_command_(const std::string &cmd);
  void process_notify_line_(const std::string &line);
  void handle_rcv_(const std::string &line);
  uint8_t bandwidth_to_code_() const;
  void publish_ready_(bool ready);
  static const char *step_to_string_(SetupStep step);

  espbt::ESPBTUUID service_uuid_;
  espbt::ESPBTUUID char_uuid_;
  espbt::ESPBTUUID notify_char_uuid_;
  uint16_t write_char_handle_{0};
  uint16_t notify_char_handle_{0};
  esp_gatt_write_type_t write_type_{ESP_GATT_WRITE_TYPE_NO_RSP};

  uint16_t address_{0};
  uint32_t frequency_{915000000};
  uint8_t spreading_factor_{9};
  uint32_t bandwidth_{125000};
  uint8_t coding_rate_{1};
  uint8_t preamble_length_{12};
  uint8_t network_id_{18};
  uint8_t tx_power_{22};
  bool compute_air_time_{false};
  uint32_t min_tx_interval_ms_{1000};
  uint32_t last_send_attempt_at_{0};

  SetupStep step_{SetupStep::DISCOVER};
  uint32_t step_sent_at_{0};
  uint8_t step_retries_{0};

  std::string rx_buffer_{};

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *lora_ready_binary_sensor_{nullptr};
#endif
  std::vector<RYLR999BLEListener *> listeners_{};
};

}  // namespace esphome::rylr999_ble

#endif
