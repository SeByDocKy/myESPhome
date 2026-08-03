#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"

#if __has_include("esphome/components/sx126x/sx126x.h")
#include "esphome/components/sx126x/sx126x.h"
#define USE_SX126X 1
#endif

#if __has_include("esphome/components/sx127x/sx127x.h")
#include "esphome/components/sx127x/sx127x.h"
#define USE_SX127X 1
#endif

#include <vector>
#include <string>
#include <unordered_set>

namespace esphome {
namespace meshcore {

// --- Constantes du protocole MeshCore (src/MeshCore.h / src/Packet.h) ---
// Voir NOTES_PROTOCOLE.md pour les sources exactes de chaque valeur.
static const uint8_t PH_ROUTE_MASK = 0x03;
static const uint8_t PH_TYPE_SHIFT = 2;
static const uint8_t PH_TYPE_MASK = 0x0F;
static const uint8_t PH_VER_SHIFT = 6;

static const uint8_t ROUTE_TYPE_TRANSPORT_FLOOD = 0x00;
static const uint8_t ROUTE_TYPE_FLOOD = 0x01;
static const uint8_t ROUTE_TYPE_DIRECT = 0x02;
static const uint8_t ROUTE_TYPE_TRANSPORT_DIRECT = 0x03;

static const uint8_t PAYLOAD_TYPE_REQ = 0x00;
static const uint8_t PAYLOAD_TYPE_RESPONSE = 0x01;
static const uint8_t PAYLOAD_TYPE_TXT_MSG = 0x02;
static const uint8_t PAYLOAD_TYPE_ACK = 0x03;
static const uint8_t PAYLOAD_TYPE_ADVERT = 0x04;
static const uint8_t PAYLOAD_TYPE_GRP_TXT = 0x05;
static const uint8_t PAYLOAD_TYPE_GRP_DATA = 0x06;
static const uint8_t PAYLOAD_TYPE_ANON_REQ = 0x07;
static const uint8_t PAYLOAD_TYPE_PATH = 0x08;
static const uint8_t PAYLOAD_TYPE_TRACE = 0x09;
static const uint8_t PAYLOAD_TYPE_MULTIPART = 0x0A;
static const uint8_t PAYLOAD_TYPE_CONTROL = 0x0B;
static const uint8_t PAYLOAD_TYPE_RAW_CUSTOM = 0x0F;

static const size_t CIPHER_KEY_SIZE = 16;   // taille de cle AES-128
static const size_t CIPHER_SECRET_SIZE = 32;  // GroupChannel::secret[32] (cle HMAC complete)
static const size_t CIPHER_MAC_SIZE = 2;    // MAC tronque (HMAC-SHA256 tronque a 2 octets)
static const size_t MAX_PATH_SIZE = 64;
static const size_t MAX_PACKET_PAYLOAD = 184;
static const size_t MAX_TRANS_UNIT = 255;

// Un canal de groupe MeshCore : nom "humain" + secret partage.
// Equivalent du "Channel" du composant meshtastic, mais le secret fait
// toujours 32 octets en interne (les 16 derniers sont a zero si la PSK
// fournie ne fait que 16 octets) car c'est ce format que MeshCore utilise
// tel quel comme cle HMAC-SHA256 (voir BaseChatMesh::addChannel).
class Channel {
 public:
  void set_name(const std::string &name) { this->name_ = name; }
  const std::string &get_name() const { return this->name_; }

  // psk : 16 ou 32 octets bruts (deja decodes du base64 par __init__.py)
  void set_psk(const std::vector<uint8_t> &psk);

  const uint8_t *secret() const { return this->secret_; }
  uint8_t hash() const { return this->hash_; }

 private:
  std::string name_;
  uint8_t secret_[CIPHER_SECRET_SIZE] = {0};
  uint8_t hash_ = 0;
};

// Petit historique anti-doublon pour le flood routing : on retient le hash
// des derniers paquets vus pendant quelques minutes, comme le fait
// MeshTables/PacketHistory cote Meshtastic (et StaticPoolPacketManager cote
// MeshCore officiel, en plus complet).
struct SeenPacket {
  uint32_t hash;
  uint32_t rx_time_ms;
};

class MeshCore : public Component
#ifdef USE_SX126X
    ,
                 sx126x::SX126xListener
#endif
#ifdef USE_SX127X
    ,
                 sx127x::SX127xListener
#endif
{
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

#ifdef USE_SX126X
  void set_lora(sx126x::SX126x *sx);
#endif
#ifdef USE_SX127X
  void set_lora(sx127x::SX127x *sx);
#endif

  void set_node_name(const std::string &name) { this->node_name_ = name; }
  void set_node_hash(uint8_t hash) { this->node_hash_ = hash; this->node_hash_set_ = true; }
  void set_hop_limit(uint8_t hop_limit) { this->hop_limit_ = hop_limit; }
  void set_repeat_enabled(bool repeat) { this->repeat_enabled_ = repeat; }
  void add_channel(const std::string &name, const std::vector<uint8_t> &psk);

  void add_listener(std::function<void(std::string, std::string, std::string, float, float)> &&listener) {
    this->packet_listeners_.add(std::move(listener));
  }

  // Envoie un message de groupe chiffre (PAYLOAD_TYPE_GRP_TXT) sur le canal
  // "channel_name". Le texte final envoye sur l'air est "node_name: text",
  // exactement le format attendu par le protocole MeshCore pour ce type de
  // paquet (voir docs/payloads.md, section "Group text message").
  bool send_group_text(const std::string &channel_name, const std::string &text);

  // Callback appele par sx126x/sx127x quand un paquet LoRa brut est recu.
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr);

 protected:
  CallbackManager<void(std::string channel, std::string from_name, std::string text, float rssi, float snr)>
      packet_listeners_{};

#ifdef USE_SX126X
  sx126x::SX126x *sx126x_ = nullptr;
#endif
#ifdef USE_SX127X
  sx127x::SX127x *sx127x_ = nullptr;
#endif

  std::string node_name_ = "esphome-meshcore";
  uint8_t node_hash_ = 0;
  bool node_hash_set_ = false;
  uint8_t hop_limit_ = 3;
  bool repeat_enabled_ = true;

  std::vector<Channel> channels_;
  std::unordered_set<uint32_t> seen_hashes_;  // simplifie : pas d'expiration pour l'instant, voir NOTES

  Channel *get_channel_by_name(const std::string &name);
  Channel *get_channel_by_hash(uint8_t hash);

  bool transmit_raw_packet(const std::vector<uint8_t> &packet);
  void handle_group_text(const uint8_t *payload, size_t payload_len, float rssi, float snr);
  void maybe_repeat_flood(uint8_t header, const uint8_t *path, size_t path_len, const uint8_t *payload,
                           size_t payload_len);

  static uint32_t hash_payload(uint8_t payload_type, const uint8_t *payload, size_t len);

  // --- Primitives crypto (AES-128 + SHA-256 auto-suffisants, voir
  // aes_sha256.h) equivalentes a Utils::encryptThenMAC /
  // Utils::MACThenDecrypt du firmware MeshCore officiel (AES-128-ECB avec
  // bourrage a zero + HMAC-SHA256 tronque a 2 octets, "encrypt-then-MAC").
  static size_t encrypt_then_mac(const uint8_t *secret32, const uint8_t *plaintext, size_t len, uint8_t *out);
  static int mac_then_decrypt(const uint8_t *secret32, const uint8_t *in, size_t len, uint8_t *out);
};

}  // namespace meshcore
}  // namespace esphome
