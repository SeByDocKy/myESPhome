// rylr998.cpp — version corrigée
// Corrections appliquées (voir analyse) :
//   [C1] rx_buffer_ non bornée              → garde RX_BUF_MAX_LEN dans loop() et send_command_()
//   [C2] send_command_() bloquant            → App.feed_wdt() à chaque itération
//   [C3] std::stoi() exceptions désactivées → safe_parse_int_() / safe_parse_hex_byte_()
//   [H1] air-time : bw_code ≠ BW(Hz)        → utilise this->bandwidth_ (float Hz)
//   [H2] lora_air_time_ uint16_t overflow   → uint32_t (+ changement miroir dans .h)
//   [M1] 240 allocs heap / paquet RX        → pointeur direct + data.reserve()
//   [M3] send_raw_() alloc std::string      → 2× write_str(), signature const char *
//   [L2] size_t underflow snr_comma == 0    → vérification explicite avant rfind()

#include "rylr998.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"   // [C2] App.feed_wdt()
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"

#define COMMAND_DELAY_MS   100
#define TIMEOUT_MS        1000
#define MAX_PAYLOAD        240
#define HEX_BUF_SIZE       496   // (240+8)*2 + 1
#define SEND_CMD_SIZE      520
#define RX_BUF_MAX_LEN     600   // [C1] garde anti-OOM : limite la taille de rx_buffer_

namespace esphome {
namespace rylr998 {

static const char *const TAG = "rylr998";

// ── Helpers de parsing sûrs ──────────────────────────────────────────────────
// [C3] Remplacent std::stoi() qui appelle std::terminate() quand les
//      exceptions C++ sont désactivées (CONFIG_COMPILER_CXX_EXCEPTIONS=n).

// Parseur entier signé : retourne false et laisse `out` inchangé si invalide.
static bool safe_parse_int_(const std::string &s, int &out) {
  if (s.empty()) return false;
  char *endptr = nullptr;
  long val = strtol(s.c_str(), &endptr, 10);
  // endptr doit avoir avancé jusqu'à la fin de la chaîne
  if (endptr == s.c_str() || *endptr != '\0') return false;
  out = static_cast<int>(val);
  return true;
}

// Parseur hex sur exactement 2 caractères (pas d'allocation heap).
// [C3 + M1] : utilisé à la place de stoi(data_str.substr(i, 2), nullptr, 16)
static bool safe_parse_hex_byte_(const char *p, uint8_t &out) {
  char buf[3] = { p[0], p[1], '\0' };
  char *endptr = nullptr;
  unsigned long val = strtoul(buf, &endptr, 16);
  if (endptr != buf + 2) return false;   // l'un des chars n'est pas un hex valide
  out = static_cast<uint8_t>(val);
  return true;
}

// Helper trim (inchangé)
static std::string trim_(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// ── setup() ─────────────────────────────────────────────────────────────────

void RYLR998Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RYLR998...");

  // [C2] Découpage du delay(500) initial pour alimenter le WDT entre les deux moitiés.
  //      setup() bloque le scheduler coopératif ESPHome — chaque composant actif
  //      (Modbus, RS232…) ne reçoit aucun cycle pendant toute la durée de setup().
  //      L'implémentation non-bloquante complète nécessiterait une machine d'états
  //      dans loop() ; App.feed_wdt() est la correction minimale urgente.
  delay(200);
  App.feed_wdt();
  delay(300);

  if (!this->send_command_("AT", TIMEOUT_MS)) {
    ESP_LOGE(TAG, "Failed to communicate with RYLR998 module");
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "Configuring module...");

  char address_cmd[32];
  snprintf(address_cmd, sizeof(address_cmd), "AT+ADDRESS=%d", this->address_);
  if (!this->send_command_(address_cmd, TIMEOUT_MS)) {
    ESP_LOGW(TAG, "Failed to set address");
  }
  delay(COMMAND_DELAY_MS);
  App.feed_wdt();   // [C2]

  char freq_cmd[32];
  snprintf(freq_cmd, sizeof(freq_cmd), "AT+BAND=%lu", this->frequency_);
  if (!this->send_command_(freq_cmd, TIMEOUT_MS)) {
    ESP_LOGW(TAG, "Failed to set frequency");
  }
  delay(COMMAND_DELAY_MS);
  App.feed_wdt();   // [C2]

  uint8_t bw_code = this->bandwidth_to_code_(this->bandwidth_);
  char param_cmd[64];
  snprintf(param_cmd, sizeof(param_cmd), "AT+PARAMETER=%d,%d,%d,%d",
           this->spreading_factor_, bw_code, this->coding_rate_, this->preamble_length_);
  if (!this->send_command_(param_cmd, TIMEOUT_MS)) {
    ESP_LOGW(TAG, "Failed to set RF parameters");
  }
  delay(COMMAND_DELAY_MS);
  App.feed_wdt();   // [C2]

  char network_cmd[32];
  snprintf(network_cmd, sizeof(network_cmd), "AT+NETWORKID=%d", this->network_id_);
  if (!this->send_command_(network_cmd, TIMEOUT_MS)) {
    ESP_LOGW(TAG, "Failed to set network ID");
  }
  delay(COMMAND_DELAY_MS);
  App.feed_wdt();   // [C2]

  // [VARIANT] Garde-fou : ne jamais dépasser le max RF du PA de la variante ciblée.
  uint8_t max_power = this->max_tx_power_();
  if (this->tx_power_ > max_power) {
    ESP_LOGW(TAG, "tx_power (%d dBm) dépasse le max %d dBm pour %s, valeur bridée",
             this->tx_power_, max_power, this->variant_name_());
    this->tx_power_ = max_power;
  }

  char power_cmd[32];
  snprintf(power_cmd, sizeof(power_cmd), "AT+CRFOP=%d", this->tx_power_);
  if (!this->send_command_(power_cmd, TIMEOUT_MS)) {
    ESP_LOGW(TAG, "Failed to set TX power");
  }
  delay(COMMAND_DELAY_MS);
  App.feed_wdt();   // [C2]

  this->initialized_ = true;
  ESP_LOGCONFIG(TAG, "RYLR998 setup complete");

  if (this->tx_power_number_ != nullptr) {
    this->tx_power_number_->publish_state(static_cast<float>(this->tx_power_));
  }
  if (this->last_error_sensor_ != nullptr) {
    this->last_error_sensor_->publish_state(0.0f);
  }

  if (this->compute_air_time_) {
    // [H1] CORRECTION : utiliser this->bandwidth_ en Hz, PAS bw_code (7/8/9).
    //      Formule LoRa : Tsym = 2^SF / BW_Hz  (résultat en secondes)
    //      Avec bw_code=7 pour 125 kHz : l'ancienne formule donnait Tsym ×17,8 trop grand.
    float bw_hz   = static_cast<float>(this->bandwidth_);   // ex : 125 000,0 Hz
    float Tsym    = powf(2.0f, static_cast<float>(this->spreading_factor_)) / bw_hz;
    float Tpreamble = (static_cast<float>(this->preamble_length_) + 4.25f) * Tsym;

    float temp =
        (8.0f * 248.0f   // (MAX_PAYLOAD=240 + header=8) octets
        - 4.0f * static_cast<float>(this->spreading_factor_)
        + 28.0f
        + 16.0f * 1.0f   // CRC activé
        - 20.0f * 0.0f); // implicit header désactivé

    float denom = 4.0f * static_cast<float>(this->spreading_factor_);
    float symb_f = ceilf(temp / denom) * static_cast<float>(this->coding_rate_ + 4);
    uint32_t payloadSymbNb = 8u + static_cast<uint32_t>(fmaxf(symb_f, 0.0f));
    float Tpayload = static_cast<float>(payloadSymbNb) * Tsym;

    // [H2] CORRECTION : stocker en uint32_t (déclaré dans .h).
    //      L'ancienne valeur uint16_t débordait silencieusement pour les SF élevés.
    this->lora_air_time_ = static_cast<uint32_t>((Tpreamble + Tpayload) * 1000.0f);
    ESP_LOGCONFIG(TAG, "RYLR998 computed LoRa air time: %lu ms", this->lora_air_time_);
  }
}

// ── loop() ──────────────────────────────────────────────────────────────────

void RYLR998Component::loop() {
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);

    if (c == '\n') {
      if (!this->rx_buffer_.empty()) {
        if (this->rx_buffer_.back() == '\r') {
          this->rx_buffer_.pop_back();
        }
        if (!this->rx_buffer_.empty()) {
          this->process_rx_line_(this->rx_buffer_);
        }
        this->rx_buffer_.clear();
      }
    } else {
      // [C1] CORRECTION : borne supérieure sur rx_buffer_.
      //      Sans cette garde, un module qui envoie du bruit sans '\n'
      //      (redémarrage à chaud, trame corrompue) fait grossir le buffer
      //      sans limite jusqu'à l'OOM, provoquant un crash dans un composant
      //      innocent (Modbus, MQTT…) bien après la cause réelle.
      if (this->rx_buffer_.size() < RX_BUF_MAX_LEN) {
        this->rx_buffer_ += static_cast<char>(c);
      } else {
        ESP_LOGW(TAG, "RX buffer overflow (%u bytes) — line discarded",
                 (unsigned) this->rx_buffer_.size());
        this->rx_buffer_.clear();
      }
    }
  }
}

// ── dump_config() ────────────────────────────────────────────────────────────

void RYLR998Component::dump_config() {
  ESP_LOGCONFIG(TAG, "RYLR998:");
  ESP_LOGCONFIG(TAG, "  Variant: %s",         this->variant_name_());
  ESP_LOGCONFIG(TAG, "  Address: %d",         this->address_);
  ESP_LOGCONFIG(TAG, "  Frequency: %lu Hz",   this->frequency_);
  ESP_LOGCONFIG(TAG, "  Spreading Factor: %d", this->spreading_factor_);
  // [minor] bandwidth_to_string_() retourne const char* → plus d'alloc heap ici
  ESP_LOGCONFIG(TAG, "  Bandwidth: %s",       this->bandwidth_to_string_(this->bandwidth_));
  ESP_LOGCONFIG(TAG, "  Coding Rate: 4/%d",   this->coding_rate_ + 4);
  ESP_LOGCONFIG(TAG, "  Preamble Length: %d", this->preamble_length_);
  ESP_LOGCONFIG(TAG, "  Network ID: %d",      this->network_id_);
  ESP_LOGCONFIG(TAG, "  TX Power: %d dBm",    this->tx_power_);
  ESP_LOGCONFIG(TAG, "  LoRa air time: %lu ms", this->lora_air_time_);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with RYLR998 failed!");
  }
}

// ── send_command_() ──────────────────────────────────────────────────────────

bool RYLR998Component::send_command_(const std::string &command, uint32_t timeout_ms) {
  // Vide le buffer UART entrant
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
  }
  this->rx_buffer_.clear();

  // [M3] CORRECTION : deux write_str() séparés au lieu de std::string + "\r\n".
  //      L'ancienne concaténation créait une copie ~520 octets sur le heap
  //      (immédiatement libérée, mais fragmentation à chaque TX).
  this->write_str(command.c_str());
  this->write_str("\r\n");
  this->flush();

  ESP_LOGV(TAG, "TX: %s", command.c_str());

  uint32_t start = millis();
  std::string line;

  while (millis() - start < timeout_ms) {
    while (this->available()) {
      uint8_t c;
      this->read_byte(&c);

      if (c == '\n') {
        if (!line.empty() && line.back() == '\r') {
          line.pop_back();
        }
        ESP_LOGV(TAG, "RX cmd: %s", line.c_str());
        if (line.find("+OK")    != std::string::npos ||
            line.find("+READY") != std::string::npos) {
          return true;
        }
        if (line.find("+ERR") != std::string::npos) {
          ESP_LOGW(TAG, "Module returned error: %s", line.c_str());
          return false;
        }
        line.clear();
      } else {
        // [C1] Même garde sur le buffer local de send_command_()
        if (line.size() < RX_BUF_MAX_LEN) {
          line += static_cast<char>(c);
        }
      }
    }
    // [C2] CORRECTION : App.feed_wdt() avant delay().
    //      Le scheduler ESPHome étant coopératif, aucun composant ne tourne
    //      pendant cette boucle. Le WDT est au minimum alimenté pour éviter
    //      un reboot silencieux sur les configurations ESP-IDF strictes.
    App.feed_wdt();
    delay(5);
  }

  ESP_LOGW(TAG, "Timeout waiting for +OK after: %s", command.c_str());
  return false;
}

// ── process_rx_line_() ───────────────────────────────────────────────────────

void RYLR998Component::process_rx_line_(const std::string &line) {
  ESP_LOGV(TAG, "RX: '%s'", line.c_str());

  if (line.find("+OK") != std::string::npos) {
    ESP_LOGV(TAG, "TX acknowledged");
    return;
  }

  if (line.find("+ERR=") != std::string::npos) {
    size_t eq = line.find("+ERR=");
    int err_code = 0;
    if (eq != std::string::npos) {
      std::string num_str = trim_(line.substr(eq + 5));
      // [C3] CORRECTION : safe_parse_int_() ne lève pas d'exception.
      //      std::stoi() appelait std::terminate() si num_str n'était pas
      //      un entier valide (bruit UART, trame tronquée, etc.).
      safe_parse_int_(num_str, err_code);   // err_code reste 0 si invalide
    }

    if (this->last_error_sensor_ != nullptr) {
      this->last_error_sensor_->publish_state(static_cast<float>(err_code));
    }
    if (err_code == 17) {
      ESP_LOGD(TAG, "TX busy (ERR=17), dropped - PacketTransport will retry");
    } else {
      ESP_LOGW(TAG, "Module error +ERR=%d: %s", err_code, line.c_str());
    }
    return;
  }

  if (line.find("+RCV=") != 0) {
    return;
  }

  std::string payload = line.substr(5);   // saute "+RCV="

  // Parsing robuste : cherche SNR et RSSI depuis la fin (data peut contenir des virgules)
  size_t snr_comma = payload.rfind(',');
  if (snr_comma == std::string::npos) return;

  // [L2] CORRECTION : snr_comma == 0 provoquait un underflow size_t (unsigned wraparound)
  //      qui passait SIZE_MAX à rfind(), parcourant toute la chaîne — UB potentiel.
  if (snr_comma == 0) {
    ESP_LOGW(TAG, "Malformed RCV (snr_comma=0): '%s'", line.c_str());
    return;
  }

  size_t rssi_comma = payload.rfind(',', snr_comma - 1);
  if (rssi_comma == std::string::npos) return;

  size_t addr_comma = payload.find(',');
  if (addr_comma == std::string::npos) return;

  size_t len_comma = payload.find(',', addr_comma + 1);
  if (len_comma == std::string::npos) return;

  // [L2] Cohérence des positions : rssi_comma doit être après len_comma
  if (rssi_comma <= len_comma) {
    ESP_LOGW(TAG, "Malformed RCV field order: '%s'", line.c_str());
    return;
  }

  // [C3] CORRECTION : safe_parse_int_() partout où std::stoi() était utilisé.
  //      Un paquet LoRa malformé ou du bruit UART pouvait crasher le firmware.
  int address_i = 0, rssi_i = 0, snr_i = 0;

  if (!safe_parse_int_(trim_(payload.substr(0, addr_comma)), address_i)) {
    ESP_LOGW(TAG, "Invalid address in RCV: '%s'", line.c_str());
    return;
  }

  std::string data_str = trim_(payload.substr(len_comma + 1, rssi_comma - len_comma - 1));

  if (!safe_parse_int_(trim_(payload.substr(rssi_comma + 1, snr_comma - rssi_comma - 1)), rssi_i)) {
    ESP_LOGW(TAG, "Invalid RSSI in RCV: '%s'", line.c_str());
    return;
  }
  if (!safe_parse_int_(trim_(payload.substr(snr_comma + 1)), snr_i)) {
    ESP_LOGW(TAG, "Invalid SNR in RCV: '%s'", line.c_str());
    return;
  }

  uint16_t address = static_cast<uint16_t>(address_i);
  int rssi = rssi_i;
  int snr  = snr_i;

  // [M1] CORRECTION : décodage hex sans substr() en boucle.
  //      L'ancienne implémentation faisait data_str.substr(i, 2) pour chaque
  //      octet, soit jusqu'à 240 allocations heap de 2 octets → fragmentation
  //      sévère sur un ESP32 qui tourne déjà Modbus + RS232 + MQTT.
  //      Ici : pointeur arithmétique direct sur le buffer de data_str, zéro alloc.
  std::vector<uint8_t> data;

  if (data_str.size() % 2 == 0 && !data_str.empty()) {
    data.reserve(data_str.size() / 2);   // [M1] évite les réallocations du vector
    const char *p = data_str.c_str();
    bool valid = true;
    for (size_t i = 0; i < data_str.size(); i += 2) {
      uint8_t byte_val = 0;
      if (!safe_parse_hex_byte_(p + i, byte_val)) {
        ESP_LOGW(TAG, "Invalid hex byte at offset %u in RCV data", (unsigned) i);
        valid = false;
        break;
      }
      data.push_back(byte_val);
    }
    if (!valid) return;
  } else if (!data_str.empty()) {
    // Longueur impaire : fallback raw ASCII (comportement inchangé)
    data.assign(data_str.begin(), data_str.end());
  }

  ESP_LOGD(TAG, "RCV from %d: %d bytes (RSSI: %d, SNR: %d)",
           address, (int) data.size(), rssi, snr);

  float rssi_f = static_cast<float>(rssi);
  float snr_f  = static_cast<float>(snr);

  if (this->raw_cb_ != nullptr) {
    this->raw_cb_(data, rssi_f, snr_f);
  }
  if (this->packet_trigger_ != nullptr) {
    this->packet_trigger_->trigger(data, rssi_f, snr_f);
  }
  this->packet_callback_.call(address, data, rssi, snr);

  if (this->rssi_sensor_ != nullptr) this->rssi_sensor_->publish_state(rssi_f);
  if (this->snr_sensor_  != nullptr) this->snr_sensor_->publish_state(snr_f);
}

// ── transmit_packet() ────────────────────────────────────────────────────────

bool RYLR998Component::transmit_packet(const std::vector<uint8_t> &data) {
  return this->transmit_packet(0, data);
}

bool RYLR998Component::transmit_packet(uint16_t destination, const std::vector<uint8_t> &data) {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "Module not initialized");
    return false;
  }
  if (data.size() > MAX_PAYLOAD) {
    ESP_LOGE(TAG, "Data too large (max 240 bytes), got %d", (int) data.size());
    return false;
  }

  // [H2] lora_air_time_ est maintenant uint32_t — comparaison uint32_t vs uint32_t (millis())
  uint32_t now = millis();
  if (now - this->last_tx_time_ < this->lora_air_time_) {
    return true;   // drop silencieux, PacketTransport retentera
  }
  this->last_tx_time_ = now;

  // [L1] Note : hex_buf (496 B) + send_cmd (520 B) = 1016 B sur la stack.
  //      L'ESP32 dispose de ~8 KB par tâche — acceptable si la profondeur
  //      d'appel reste raisonnable (eviter les lambdas imbriquées profondes).
  char hex_buf[HEX_BUF_SIZE];
  for (size_t i = 0; i < data.size(); i++) {
    snprintf(hex_buf + i * 2, 3, "%02X", data[i]);
  }
  hex_buf[data.size() * 2] = '\0';

  char send_cmd[SEND_CMD_SIZE];
  snprintf(send_cmd, sizeof(send_cmd), "AT+SEND=%d,%d,%s",
           destination, (int) (data.size() * 2), hex_buf);

  ESP_LOGD(TAG, "TX to %d (%d bytes): %s", destination, (int) data.size(), hex_buf);
  this->send_raw_(send_cmd);
  return true;
}

// ── send_raw_() ──────────────────────────────────────────────────────────────

// [M3] CORRECTION : signature const char * au lieu de const std::string &.
//      L'ancienne implémentation faisait `std::string full = command + "\r\n"`
//      créant une copie ~520 octets sur le heap à chaque appel, immédiatement
//      libérée. Deux write_str() séparés atteignent le même résultat sans alloc.
//      ATTENTION : la déclaration dans rylr998.h doit aussi être mise à jour :
//        void send_raw_(const char *command);
void RYLR998Component::send_raw_(const char *command) {
  this->write_str(command);
  this->write_str("\r\n");
  this->flush();
}

// ── send_data() ──────────────────────────────────────────────────────────────

bool RYLR998Component::send_data(uint16_t destination, const std::vector<uint8_t> &data) {
  return this->transmit_packet(destination, data);
}

bool RYLR998Component::send_data(uint16_t destination, const std::string &data) {
  return this->transmit_packet(destination, std::vector<uint8_t>(data.begin(), data.end()));
}

// ── apply_tx_power() ─────────────────────────────────────────────────────────

void RYLR998Component::apply_tx_power(uint8_t power) {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "apply_tx_power: module not yet initialized, ignoring");
    return;
  }

  // [VARIANT] Même garde-fou qu'en setup() : une valeur reçue via l'entité
  //           number ou une automation ne doit pas dépasser le max RF du module.
  uint8_t max_power = this->max_tx_power_();
  if (power > max_power) {
    ESP_LOGW(TAG, "Requested tx_power %d dBm exceeds %s max (%d dBm), clamping",
             power, this->variant_name_(), max_power);
    power = max_power;
  }
  this->tx_power_ = power;

  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+CRFOP=%d", power);
  ESP_LOGD(TAG, "Setting TX power to %d dBm", power);

  // [M2] AVERTISSEMENT : send_command_() bloque jusqu'à TIMEOUT_MS (1 s) ici.
  //      Si apply_tx_power() est appelé depuis une automation ou un lambda
  //      déclenché dans loop(), TOUS les composants ESPHome (Modbus, RS232,
  //      HMS…) sont gelés pendant cette durée.
  //      Correction complète : utiliser une file de commandes pendantes traitée
  //      de manière non-bloquante dans loop() avec une machine d'états.
  //      Pour l'instant, App.feed_wdt() est au moins appelé dans send_command_().
  if (!this->send_command_(cmd, TIMEOUT_MS)) {
    ESP_LOGW(TAG, "Failed to set TX power to %d dBm", power);
    return;
  }
  ESP_LOGI(TAG, "TX power set to %d dBm", power);
}

// ── Helpers bande passante ───────────────────────────────────────────────────

uint8_t RYLR998Component::bandwidth_to_code_(uint32_t bandwidth) {
  switch (bandwidth) {
    case 125000: return 7;
    case 250000: return 8;
    case 500000: return 9;
    default:
      ESP_LOGW(TAG, "Unknown bandwidth %lu Hz, defaulting to 125 kHz", bandwidth);
      return 7;
  }
}

// [minor] Retourne const char * pour éviter l'alloc heap dans dump_config().
//         La déclaration dans rylr998.h doit aussi être mise à jour :
//           const char *bandwidth_to_string_(uint32_t bandwidth);
const char *RYLR998Component::bandwidth_to_string_(uint32_t bandwidth) {
  switch (bandwidth) {
    case 125000: return "125 kHz";
    case 250000: return "250 kHz";
    case 500000: return "500 kHz";
    default:     return "Unknown";
  }
}

}  // namespace rylr998
}  // namespace esphome
