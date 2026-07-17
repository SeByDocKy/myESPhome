#include "rylr999_ble.h"

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP32

namespace esphome::rylr999_ble {

static const char *const TAG = "rylr999_ble";

namespace {

std::string hex_encode(const std::vector<uint8_t> &data) {
  static const char *const kHex = "0123456789abcdef";
  std::string out;
  out.reserve(data.size() * 2);
  for (uint8_t b : data) {
    out.push_back(kHex[b >> 4]);
    out.push_back(kHex[b & 0x0F]);
  }
  return out;
}

bool hex_decode(const std::string &hex, std::vector<uint8_t> &out) {
  if (hex.empty() || hex.size() % 2 != 0)
    return false;
  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  };
  out.clear();
  out.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    int hi = nibble(hex[i]);
    int lo = nibble(hex[i + 1]);
    if (hi < 0 || lo < 0)
      return false;
    out.push_back(static_cast<uint8_t>((hi << 4) | lo));
  }
  return true;
}

}  // namespace

const char *RYLR999BLEComponent::step_to_string_(SetupStep step) {
  switch (step) {
    case SetupStep::DISCOVER:
      return "DISCOVER";
    case SetupStep::ADDRESS:
      return "AT+ADDRESS";
    case SetupStep::BAND:
      return "AT+BAND";
    case SetupStep::PARAMETER:
      return "AT+PARAMETER";
    case SetupStep::NETWORKID:
      return "AT+NETWORKID";
    case SetupStep::CRFOP:
      return "AT+CRFOP";
    case SetupStep::READY:
      return "READY";
    case SetupStep::FAILED:
      return "FAILED";
  }
  return "?";
}

uint8_t RYLR999BLEComponent::bandwidth_to_code_() const {
  switch (this->bandwidth_) {
    case 250000:
      return 8;
    case 500000:
      return 9;
    default:
      return 7;  // 125kHz, valeur par défaut REYAX
  }
}

void RYLR999BLEComponent::publish_ready_(bool ready) {
#ifdef USE_BINARY_SENSOR
  if (this->lora_ready_binary_sensor_ != nullptr)
    this->lora_ready_binary_sensor_->publish_state(ready);
#endif
}

void RYLR999BLEComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RYLR999 (configuration LoRa via BLE):");
  ESP_LOGCONFIG(TAG, "  Adresse LoRa: %u", this->address_);
  ESP_LOGCONFIG(TAG, "  Fréquence: %u Hz", (unsigned) this->frequency_);
  ESP_LOGCONFIG(TAG, "  Spreading factor: %u", this->spreading_factor_);
  ESP_LOGCONFIG(TAG, "  Bandwidth: %u Hz", (unsigned) this->bandwidth_);
  ESP_LOGCONFIG(TAG, "  Coding rate: %u", this->coding_rate_);
  ESP_LOGCONFIG(TAG, "  Preamble: %u", this->preamble_length_);
  ESP_LOGCONFIG(TAG, "  Network ID: %u", this->network_id_);
  ESP_LOGCONFIG(TAG, "  TX power: %u dBm", this->tx_power_);
  ESP_LOGCONFIG(TAG, "  Taille max payload brut: %u octets (avant encodage hex)",
                (unsigned) this->get_max_packet_size());
  ESP_LOGCONFIG(TAG, "  Handle écriture: 0x%02x, handle notification: 0x%02x", this->write_char_handle_,
                this->notify_char_handle_);
  ESP_LOGCONFIG(TAG, "  Prêt: %s", YESNO(this->is_lora_ready()));
}

std::string RYLR999BLEComponent::build_step_command_(SetupStep step) const {
  char cmd[64];
  switch (step) {
    case SetupStep::ADDRESS:
      snprintf(cmd, sizeof(cmd), "AT+ADDRESS=%u", this->address_);
      return cmd;
    case SetupStep::BAND:
      snprintf(cmd, sizeof(cmd), "AT+BAND=%u", (unsigned) this->frequency_);
      return cmd;
    case SetupStep::PARAMETER:
      snprintf(cmd, sizeof(cmd), "AT+PARAMETER=%u,%u,%u,%u", this->spreading_factor_,
               this->bandwidth_to_code_(), this->coding_rate_, this->preamble_length_);
      return cmd;
    case SetupStep::NETWORKID:
      snprintf(cmd, sizeof(cmd), "AT+NETWORKID=%u", this->network_id_);
      return cmd;
    case SetupStep::CRFOP:
      snprintf(cmd, sizeof(cmd), "AT+CRFOP=%u", this->tx_power_);
      return cmd;
    default:
      return "";
  }
}

void RYLR999BLEComponent::advance_to_(SetupStep step) {
  this->step_ = step;
  this->step_retries_ = 0;
  this->send_current_step_();
}

void RYLR999BLEComponent::send_current_step_() {
  if (this->step_ == SetupStep::READY) {
    ESP_LOGI(TAG, "Configuration LoRa RYLR999 validée intégralement via BLE (ADDRESS, BAND, "
                  "PARAMETER, NETWORKID, CRFOP)");
    this->publish_ready_(true);
    return;
  }

  std::string cmd = this->build_step_command_(this->step_);
  if (cmd.empty())
    return;

  if (this->send_at_command_(cmd)) {
    this->step_sent_at_ = millis();
  } else {
    // On retentera au prochain passage de loop() via le timeout normal.
    this->step_sent_at_ = millis();
  }
}

bool RYLR999BLEComponent::send_at_command_(const std::string &cmd) {
  if (this->write_char_handle_ == 0) {
    ESP_LOGW(TAG, "Caractéristique BLE d'écriture pas encore résolue, commande '%s' ignorée", cmd.c_str());
    return false;
  }
  std::string full = cmd + "\r\n";
  ESP_LOGD(TAG, "BLE TX -> %s", cmd.c_str());
  esp_err_t err = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->write_char_handle_, full.size(),
      reinterpret_cast<uint8_t *>(full.data()), this->write_type_, ESP_GATT_AUTH_REQ_NONE);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gattc_write_char a échoué (%s) pour '%s'", esp_err_to_name(err), cmd.c_str());
    return false;
  }
  return true;
}

bool RYLR999BLEComponent::send_payload(const std::vector<uint8_t> &payload) {
  if (!this->is_lora_ready()) {
    ESP_LOGW(TAG, "send_payload: configuration LoRa pas encore prête, envoi ignoré");
    return false;
  }
  if (payload.size() > this->get_max_packet_size()) {
    ESP_LOGW(TAG, "send_payload: paquet de %u octets > max %u, envoi ignoré", (unsigned) payload.size(),
             (unsigned) this->get_max_packet_size());
    return false;
  }
  // packet_transport peut déclencher un envoi à la fois sur son
  // update_interval ET immédiatement à chaque changement de valeur d'un
  // sensor suivi (PacketTransport::loop()) - deux appels à send_payload()
  // peuvent donc arriver à quelques centaines de ms d'écart. Le RYLR999
  // rejette (+ERR=17) tout AT+SEND lancé avant la fin d'antenne du
  // précédent : on impose donc un espacement mini plutôt que de laisser le
  // module nous le refuser silencieusement.
  uint32_t now = millis();
  if (this->last_send_attempt_at_ != 0 && now - this->last_send_attempt_at_ < this->min_tx_interval_ms_) {
    ESP_LOGD(TAG, "send_payload: envoi différé (dernier AT+SEND il y a %u ms, minimum %u ms)",
             (unsigned) (now - this->last_send_attempt_at_), (unsigned) this->min_tx_interval_ms_);
    return false;
  }

  std::string hex = hex_encode(payload);
  char header[24];
  // Adresse LoRa de destination fixée à 0 (broadcast) : c'est packet_transport
  // qui identifie l'émetteur/le fournisseur au niveau applicatif à partir du
  // contenu du paquet, pas l'adresse radio du module.
  snprintf(header, sizeof(header), "AT+SEND=0,%u,", (unsigned) hex.size());
  this->last_send_attempt_at_ = now;
  return this->send_at_command_(std::string(header) + hex);
}

void RYLR999BLEComponent::loop() {
  switch (this->step_) {
    case SetupStep::DISCOVER:
    case SetupStep::READY:
    case SetupStep::FAILED:
      return;
    default:
      break;
  }
  if (this->step_sent_at_ == 0 || millis() - this->step_sent_at_ < SETUP_STEP_TIMEOUT_MS)
    return;

  this->step_retries_++;
  if (this->step_retries_ > SETUP_MAX_RETRIES) {
    ESP_LOGE(TAG, "Étape de configuration LoRa '%s' échouée après %u tentatives, abandon",
             step_to_string_(this->step_), (unsigned) SETUP_MAX_RETRIES);
    this->step_ = SetupStep::FAILED;
    this->publish_ready_(false);
    return;
  }
  ESP_LOGW(TAG, "Pas de +OK reçu pour '%s', nouvelle tentative (%u/%u)", step_to_string_(this->step_),
           (unsigned) this->step_retries_, (unsigned) SETUP_MAX_RETRIES);
  this->send_current_step_();
}

void RYLR999BLEComponent::handle_rcv_(const std::string &line) {
  // "+RCV=<addr>,<len>,<hex_data>,<rssi>,<snr>"
  size_t pos1 = line.find(',', 5);
  size_t pos2 = (pos1 == std::string::npos) ? std::string::npos : line.find(',', pos1 + 1);
  size_t last = line.rfind(',');
  size_t second_last = (last == std::string::npos || last == 0) ? std::string::npos : line.rfind(',', last - 1);

  if (pos1 == std::string::npos || pos2 == std::string::npos || last == std::string::npos ||
      second_last == std::string::npos || second_last <= pos2) {
    ESP_LOGW(TAG, "Trame +RCV malformée: '%s'", line.c_str());
    return;
  }

  std::string data_hex = line.substr(pos2 + 1, second_last - pos2 - 1);
  std::string rssi_str = line.substr(second_last + 1, last - second_last - 1);
  std::string snr_str = line.substr(last + 1);

  std::vector<uint8_t> payload;
  if (!hex_decode(data_hex, payload)) {
    // Une trame LoRa reçue qui n'est pas de l'hex valide ne vient pas de
    // notre propre encodage packet_transport : on l'ignore silencieusement
    // (ça peut être un autre appareil sur le même NETWORKID).
    ESP_LOGV(TAG, "+RCV ignorée (payload non-hex, probablement pas un pair packet_transport): '%s'",
             data_hex.c_str());
    return;
  }

  float rssi = strtof(rssi_str.c_str(), nullptr);
  float snr = strtof(snr_str.c_str(), nullptr);
  ESP_LOGV(TAG, "+RCV: %u octets, rssi=%.0f, snr=%.0f", (unsigned) payload.size(), rssi, snr);

  for (auto *listener : this->listeners_) {
    listener->on_packet(payload, rssi, snr);
  }
}

void RYLR999BLEComponent::process_notify_line_(const std::string &line) {
  if (line.empty())
    return;
  ESP_LOGV(TAG, "BLE RX <- %s", line.c_str());

  bool in_setup = this->step_ != SetupStep::DISCOVER && this->step_ != SetupStep::READY &&
                  this->step_ != SetupStep::FAILED;

  if (in_setup) {
    if (line == "+OK") {
      switch (this->step_) {
        case SetupStep::ADDRESS:
          this->advance_to_(SetupStep::BAND);
          return;
        case SetupStep::BAND:
          this->advance_to_(SetupStep::PARAMETER);
          return;
        case SetupStep::PARAMETER:
          this->advance_to_(SetupStep::NETWORKID);
          return;
        case SetupStep::NETWORKID:
          this->advance_to_(SetupStep::CRFOP);
          return;
        case SetupStep::CRFOP:
          this->advance_to_(SetupStep::READY);
          return;
        default:
          return;
      }
    }
    if (line.rfind("+ERR=", 0) == 0) {
      // +ERR= peut être une notification asynchrone sans rapport avec notre
      // commande (ex: +ERR=12 "CRC error" sur une réception LoRa corrompue,
      // observé en pratique interleaved avec nos propres +OK). Le protocole
      // BLE transparent du RYLR999 ne permet pas de corréler une réponse à
      // la commande qui l'a déclenchée, donc on ne considère PAS ça comme un
      // rejet de l'étape en cours : on se contente de logger, et on laisse
      // le mécanisme de retry sur timeout (loop()) être seul juge du besoin
      // de renvoyer la commande.
      ESP_LOGD(TAG, "Notification '%s' reçue pendant '%s' (probablement un événement LoRa "
                    "asynchrone sans lien avec la commande en cours)",
               line.c_str(), step_to_string_(this->step_));
      return;
    }
  }

  if (line.rfind("+RCV=", 0) == 0) {
    this->handle_rcv_(line);
  }
}

void RYLR999BLEComponent::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *write_chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
      if (write_chr == nullptr) {
        char service_buf[esp32_ble::UUID_STR_LEN];
        char char_buf[esp32_ble::UUID_STR_LEN];
        ESP_LOGE(TAG, "Caractéristique d'écriture introuvable: service %s, char %s",
                 this->service_uuid_.to_str(service_buf), this->char_uuid_.to_str(char_buf));
        break;
      }
      this->write_char_handle_ = write_chr->handle;
      if (write_chr->properties & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) {
        this->write_type_ = ESP_GATT_WRITE_TYPE_NO_RSP;
      } else if (write_chr->properties & ESP_GATT_CHAR_PROP_BIT_WRITE) {
        this->write_type_ = ESP_GATT_WRITE_TYPE_RSP;
      } else {
        ESP_LOGE(TAG, "La caractéristique BLE d'écriture ne supporte pas l'écriture (properties=0x%02x)",
                 write_chr->properties);
        break;
      }

      auto *notify_chr = this->parent()->get_characteristic(this->service_uuid_, this->notify_char_uuid_);
      if (notify_chr == nullptr) {
        char service_buf[esp32_ble::UUID_STR_LEN];
        char char_buf[esp32_ble::UUID_STR_LEN];
        ESP_LOGE(TAG, "Caractéristique de notification introuvable: service %s, char %s",
                 this->service_uuid_.to_str(service_buf), this->notify_char_uuid_.to_str(char_buf));
        break;
      }
      if (!(notify_chr->properties & (ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_INDICATE))) {
        ESP_LOGE(TAG,
                 "La caractéristique de notification (properties=0x%02x) ne supporte pas NOTIFY/INDICATE - "
                 "vérifiez notify_characteristic_uuid",
                 notify_chr->properties);
        break;
      }
      this->notify_char_handle_ = notify_chr->handle;

      auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                                       this->parent()->get_remote_bda(), notify_chr->handle);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify a échoué, status=%d", status);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.status == ESP_GATT_OK && param->reg_for_notify.handle == this->notify_char_handle_) {
        this->node_state = espbt::ClientState::ESTABLISHED;
        this->rx_buffer_.clear();
        ESP_LOGI(TAG, "Connecté au RYLR999, lancement de la configuration LoRa via BLE");
        this->advance_to_(SetupStep::ADDRESS);
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->notify_char_handle_ || param->notify.value_len == 0)
        break;
      this->rx_buffer_.append(reinterpret_cast<const char *>(param->notify.value), param->notify.value_len);
      size_t pos;
      while ((pos = this->rx_buffer_.find('\n')) != std::string::npos) {
        std::string line = this->rx_buffer_.substr(0, pos);
        this->rx_buffer_.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        this->process_notify_line_(line);
      }
      break;
    }
    case ESP_GATTC_CLOSE_EVT:
    case ESP_GATTC_DISCONNECT_EVT: {
      if (this->step_ != SetupStep::DISCOVER) {
        ESP_LOGW(TAG, "Connexion BLE au RYLR999 perdue, configuration LoRa à refaire");
      }
      this->publish_ready_(false);
      this->step_ = SetupStep::DISCOVER;
      this->write_char_handle_ = 0;
      this->notify_char_handle_ = 0;
      this->rx_buffer_.clear();
      break;
    }
    default:
      break;
  }
}

}  // namespace esphome::rylr999_ble

#endif
