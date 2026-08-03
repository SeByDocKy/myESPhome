#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "meshcore.h"
#include "aes_sha256.h"

namespace esphome {
namespace meshcore {

static const char *const TAG = "meshcore.component";

// ---------------------------------------------------------------------
// Channel : derivation du secret[32] et du hash de canal a partir de la
// PSK fournie en YAML, EXACTEMENT comme BaseChatMesh::addChannel() dans
// le firmware MeshCore officiel (src/helpers/BaseChatMesh.cpp) :
//   - secret[32] = psk (16 octets) + 16 octets a zero   -- si PSK 16 octets
//     ou secret[32] = psk telle quelle                  -- si PSK 32 octets
//   - hash = SHA256(secret, 32)[0]   (le "channel hash" mis dans le paquet)
// ---------------------------------------------------------------------
void Channel::set_psk(const std::vector<uint8_t> &psk) {
  memset(this->secret_, 0, sizeof(this->secret_));
  size_t n = std::min(psk.size(), sizeof(this->secret_));
  memcpy(this->secret_, psk.data(), n);

  uint8_t full_hash[32];
  sha256_impl::hash(this->secret_, sizeof(this->secret_), full_hash);

  this->hash_ = full_hash[0];
}

// ---------------------------------------------------------------------
// Primitives crypto : AES-128-ECB (bourrage a zero sur le dernier bloc)
// + HMAC-SHA256 tronque a 2 octets, calcule sur le texte CHIFFRE
// ("encrypt-then-MAC"), comme Utils::encryptThenMAC / Utils::MACThenDecrypt
// du firmware MeshCore (src/Utils.cpp). La cle AES est constituee des 16
// premiers octets de "secret32", la cle HMAC est "secret32" en entier
// (32 octets, potentiellement complete de zeros - voir Channel::set_psk).
//
// ATTENTION SECURITE : ce mode ECB avec un MAC calcule sur le ciphertext
// (et non pas un chiffrement authentifie moderne type AES-GCM/CCM) est
// une limitation connue et documentee du protocole MeshCore lui-meme
// (cf. issue GitHub meshcore-dev/MeshCore#259), pas une simplification
// introduite ici : ce composant reproduit fidelement le format "sur l'air"
// pour rester interoperable avec le reste du reseau MeshCore.
// ---------------------------------------------------------------------

static size_t aes128_ecb_encrypt_padded(const uint8_t *key16, const uint8_t *src, size_t len, uint8_t *dest) {
  aes128::Ctx ctx;
  aes128::set_key(&ctx, key16);

  size_t out_len = 0;
  while (len >= 16) {
    aes128::encrypt_block(&ctx, src, dest + out_len);
    src += 16;
    len -= 16;
    out_len += 16;
  }
  if (len > 0) {
    uint8_t block[16] = {0};
    memcpy(block, src, len);
    aes128::encrypt_block(&ctx, block, dest + out_len);
    out_len += 16;
  }
  return out_len;
}

static void aes128_ecb_decrypt(const uint8_t *key16, const uint8_t *src, size_t len, uint8_t *dest) {
  aes128::Ctx ctx;
  aes128::set_key(&ctx, key16);
  for (size_t off = 0; off + 16 <= len; off += 16) {
    aes128::decrypt_block(&ctx, src + off, dest + off);
  }
}

static void hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len,
                         uint8_t out[32]) {
  sha256_impl::hmac(key, key_len, data, data_len, out);
}

// out doit pouvoir contenir CIPHER_MAC_SIZE + round_up16(len) octets.
size_t MeshCore::encrypt_then_mac(const uint8_t *secret32, const uint8_t *plaintext, size_t len, uint8_t *out) {
  size_t enc_len = aes128_ecb_encrypt_padded(secret32, plaintext, len, out + CIPHER_MAC_SIZE);

  uint8_t full_hmac[32];
  hmac_sha256(secret32, CIPHER_SECRET_SIZE, out + CIPHER_MAC_SIZE, enc_len, full_hmac);
  memcpy(out, full_hmac, CIPHER_MAC_SIZE);

  return CIPHER_MAC_SIZE + enc_len;
}

// Retourne la longueur en clair (multiple de 16, peut contenir du bourrage
// zero final a ignorer par l'appelant) ou -1 si le MAC est invalide.
int MeshCore::mac_then_decrypt(const uint8_t *secret32, const uint8_t *in, size_t len, uint8_t *out) {
  if (len <= CIPHER_MAC_SIZE)
    return -1;

  size_t cipher_len = len - CIPHER_MAC_SIZE;
  uint8_t full_hmac[32];
  hmac_sha256(secret32, CIPHER_SECRET_SIZE, in + CIPHER_MAC_SIZE, cipher_len, full_hmac);

  if (memcmp(full_hmac, in, CIPHER_MAC_SIZE) != 0)
    return -1;

  aes128_ecb_decrypt(secret32, in + CIPHER_MAC_SIZE, cipher_len, out);
  return static_cast<int>(cipher_len);
}

// ---------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------

#ifdef USE_SX126X
void MeshCore::set_lora(sx126x::SX126x *sx) {
  this->sx126x_ = sx;
  this->sx126x_->register_listener(this);
}
#endif
#ifdef USE_SX127X
void MeshCore::set_lora(sx127x::SX127x *sx) {
  this->sx127x_ = sx;
  this->sx127x_->register_listener(this);
}
#endif

void MeshCore::add_channel(const std::string &name, const std::vector<uint8_t> &psk) {
  Channel ch;
  ch.set_name(name);
  ch.set_psk(psk);
  ESP_LOGCONFIG(TAG, "Canal '%s' ajoute, hash=0x%02x", name.c_str(), ch.hash());
  this->channels_.push_back(ch);
}

void MeshCore::setup() {
  if (!this->node_hash_set_) {
    // Pas d'identite Ed25519 dans cette v1 (voir NOTES_PROTOCOLE.md) : on
    // derive un octet "node_hash" pseudo-unique a partir du nom du noeud,
    // uniquement utilise pour marquer notre passage dans le "path" flood
    // quand on relaie un paquet (voir maybe_repeat_flood()).
    uint8_t digest[32];
    sha256_impl::hash(reinterpret_cast<const uint8_t *>(this->node_name_.data()), this->node_name_.size(), digest);
    this->node_hash_ = digest[0];
  }
  ESP_LOGCONFIG(TAG, "MeshCore demarre : node_name='%s' node_hash=0x%02x", this->node_name_.c_str(),
                this->node_hash_);
}

void MeshCore::loop() {
  // Rien de periodique pour l'instant (pas d'advert automatique - voir
  // NOTES_PROTOCOLE.md, PAYLOAD_TYPE_ADVERT n'est pas implemente).
}

void MeshCore::dump_config() {
  ESP_LOGCONFIG(TAG, "MeshCore:");
  ESP_LOGCONFIG(TAG, "  Node name: %s", this->node_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Node hash: 0x%02x%s", this->node_hash_, this->node_hash_set_ ? "" : " (derive du nom)");
  ESP_LOGCONFIG(TAG, "  Hop limit: %u", this->hop_limit_);
  ESP_LOGCONFIG(TAG, "  Repeat (flood): %s", this->repeat_enabled_ ? "oui" : "non");
  for (auto &ch : this->channels_) {
    ESP_LOGCONFIG(TAG, "  Canal '%s' (hash=0x%02x)", ch.get_name().c_str(), ch.hash());
  }
}

Channel *MeshCore::get_channel_by_name(const std::string &name) {
  for (auto &ch : this->channels_) {
    if (ch.get_name() == name)
      return &ch;
  }
  return nullptr;
}

Channel *MeshCore::get_channel_by_hash(uint8_t hash) {
  for (auto &ch : this->channels_) {
    if (ch.hash() == hash)
      return &ch;
  }
  return nullptr;
}

// ---------------------------------------------------------------------
// Emission
// ---------------------------------------------------------------------

bool MeshCore::transmit_raw_packet(const std::vector<uint8_t> &packet) {
  if (packet.size() > MAX_TRANS_UNIT) {
    ESP_LOGE(TAG, "Paquet trop grand (%u > %u octets)", packet.size(), MAX_TRANS_UNIT);
    return false;
  }
#ifdef USE_SX126X
  if (this->sx126x_ != nullptr) {
    return this->sx126x_->transmit_packet(packet) == sx126x::SX126xError::NONE;
  }
#endif
#ifdef USE_SX127X
  if (this->sx127x_ != nullptr) {
    return this->sx127x_->transmit_packet(packet) == sx127x::SX127xError::NONE;
  }
#endif
  ESP_LOGE(TAG, "Aucune radio LoRa configuree (voir 'lora:' dans la config meshcore)");
  return false;
}

bool MeshCore::send_group_text(const std::string &channel_name, const std::string &text) {
  Channel *ch = this->get_channel_by_name(channel_name);
  if (ch == nullptr) {
    ESP_LOGE(TAG, "Canal '%s' inconnu, message non envoye", channel_name.c_str());
    return false;
  }

  // Plaintext MeshCore pour un message de groupe : timestamp(4, LE) +
  // txt_type/attempt(1) + "<nom expediteur>: <texte>"
  // (docs/payloads.md, "Plain text message" / "Group text message")
  std::string body = this->node_name_ + ": " + text;
  std::vector<uint8_t> plaintext;
  uint32_t now = static_cast<uint32_t>(time(nullptr));
  plaintext.push_back(static_cast<uint8_t>(now & 0xFF));
  plaintext.push_back(static_cast<uint8_t>((now >> 8) & 0xFF));
  plaintext.push_back(static_cast<uint8_t>((now >> 16) & 0xFF));
  plaintext.push_back(static_cast<uint8_t>((now >> 24) & 0xFF));
  plaintext.push_back(0x00);  // txt_type=plain(0) << 2 | attempt(0)
  plaintext.insert(plaintext.end(), body.begin(), body.end());

  if (plaintext.size() > MAX_PACKET_PAYLOAD) {
    ESP_LOGE(TAG, "Message trop long une fois encode (%u octets)", plaintext.size());
    return false;
  }

  // encrypt_then_mac ajoute jusqu'a 15 octets de bourrage : on prevoit large
  std::vector<uint8_t> enc(CIPHER_MAC_SIZE + plaintext.size() + 16);
  size_t enc_len = MeshCore::encrypt_then_mac(ch->secret(), plaintext.data(), plaintext.size(), enc.data());
  enc.resize(enc_len);

  // Payload GRP_TXT = channel_hash(1) + mac(2) + ciphertext(...)
  std::vector<uint8_t> payload;
  payload.push_back(ch->hash());
  payload.insert(payload.end(), enc.begin(), enc.end());

  if (payload.size() > MAX_PACKET_PAYLOAD) {
    ESP_LOGE(TAG, "Payload chiffre trop long (%u octets)", payload.size());
    return false;
  }

  // header = version(2b, =0) | payload_type(4b) | route_type(2b)
  uint8_t header = (PAYLOAD_TYPE_GRP_TXT & PH_TYPE_MASK) << PH_TYPE_SHIFT;
  header |= (ROUTE_TYPE_FLOOD & PH_ROUTE_MASK);

  std::vector<uint8_t> packet;
  packet.push_back(header);
  packet.push_back(0x00);  // path_length = 0 : paquet flood qu'on emet nous-memes (path vide au depart)
  packet.insert(packet.end(), payload.begin(), payload.end());

  // On enregistre le hash de CE paquet dans le cache anti-doublon avant de
  // l'emettre : sans ca, quand un autre noeud nous renvoie notre propre
  // message en le relayant (flood), on ne le reconnaissait pas comme deja
  // vu et on le retraitait/rediffusait a nouveau -> jusqu'a 3 copies du
  // meme message sur l'air pour un seul envoi. Ce "spam" involontaire peut
  // declencher une protection anti-flood cote appli/reseau MeshCore.
  this->seen_hashes_.insert(MeshCore::hash_payload(PAYLOAD_TYPE_GRP_TXT, payload.data(), payload.size()));

  ESP_LOGD(TAG, "Envoi GRP_TXT sur '%s' (hash=0x%02x, %u octets payload): \"%s\"", channel_name.c_str(), ch->hash(),
           payload.size(), body.c_str());

  return this->transmit_raw_packet(packet);
}

// ---------------------------------------------------------------------
// Reception
// ---------------------------------------------------------------------

uint32_t MeshCore::hash_payload(uint8_t payload_type, const uint8_t *payload, size_t len) {
  // Deduplication simplifiee : Meshcore officiel hashe (payload_type +
  // payload) en SHA-256 (voir Packet::calculatePacketHash, src/Packet.cpp).
  // On ne garde que les 4 premiers octets pour limiter la RAM utilisee par
  // seen_hashes_.
  uint8_t digest[32];
  sha256_impl::Ctx ctx;
  sha256_impl::init(&ctx);
  sha256_impl::update(&ctx, &payload_type, 1);
  sha256_impl::update(&ctx, payload, len);
  sha256_impl::final(&ctx, digest);
  return (digest[0] << 24) | (digest[1] << 16) | (digest[2] << 8) | digest[3];
}

void MeshCore::handle_group_text(const uint8_t *payload, size_t payload_len, float rssi, float snr) {
  if (payload_len < 1 + CIPHER_MAC_SIZE + 1) {
    ESP_LOGW(TAG, "Payload GRP_TXT trop court (%u octets)", payload_len);
    return;
  }

  uint8_t channel_hash = payload[0];
  Channel *ch = this->get_channel_by_hash(channel_hash);
  if (ch == nullptr) {
    // Message pour un canal qu'on ne connait pas (ou dont on n'a pas la
    // PSK) : parfaitement normal sur un reseau MeshCore partage, on ignore.
    ESP_LOGD(TAG, "GRP_TXT recu pour un canal inconnu (hash=0x%02x), ignore", channel_hash);
    return;
  }

  const uint8_t *enc = payload + 1;
  size_t enc_len = payload_len - 1;

  std::vector<uint8_t> plain(enc_len);  // toujours >= taille utile
  int plain_len = MeshCore::mac_then_decrypt(ch->secret(), enc, enc_len, plain.data());
  if (plain_len < 0) {
    ESP_LOGW(TAG, "MAC invalide pour un message sur '%s' (mauvaise PSK ou paquet corrompu)", ch->get_name().c_str());
    return;
  }

  if (plain_len < 5) {
    ESP_LOGW(TAG, "Message dechiffre trop court sur '%s'", ch->get_name().c_str());
    return;
  }

  // 4 octets timestamp (LE, non utilise ici) + 1 octet txt_type/attempt + texte
  const char *msg_start = reinterpret_cast<const char *>(plain.data() + 5);
  // Le message dechiffre peut contenir du bourrage zero final (voir
  // aes128_ecb_encrypt_padded) : on s'arrete au premier octet nul.
  size_t msg_len = 0;
  while (msg_len < static_cast<size_t>(plain_len) - 5 && msg_start[msg_len] != '\0')
    msg_len++;
  std::string message(msg_start, msg_len);

  // Format attendu "<nom expediteur>: <texte>" (non signe / non verifie,
  // voir docs/payloads.md : "any channel-key holder can choose any sender
  // name").
  std::string from_name = "?";
  std::string text = message;
  size_t sep = message.find(": ");
  if (sep != std::string::npos) {
    from_name = message.substr(0, sep);
    text = message.substr(sep + 2);
  }

  ESP_LOGD(TAG, "GRP_TXT sur '%s' de '%s': \"%s\" (rssi=%.1f snr=%.1f)", ch->get_name().c_str(), from_name.c_str(),
           text.c_str(), rssi, snr);

  this->packet_listeners_.call(ch->get_name(), from_name, text, rssi, snr);
}

void MeshCore::maybe_repeat_flood(uint8_t header, const uint8_t *path, size_t path_len, const uint8_t *payload,
                                   size_t payload_len) {
  if (!this->repeat_enabled_)
    return;
  if (path_len >= this->hop_limit_ || path_len >= MAX_PATH_SIZE)
    return;  // budget de sauts (hop_limit) atteint

  // On rebroadcast le paquet en ajoutant notre propre "node_hash" au path,
  // comme le fait un repeater MeshCore (docs/packet_format.md : "each
  // repeater appends its hash to the path"). Contrairement au firmware
  // officiel, il n'y a ici ni delai aleatoire ni gestion fine du budget
  // d'antenne (airtime) avant retransmission - voir NOTES_PROTOCOLE.md.
  for (size_t i = 0; i < path_len; i++) {
    if (path[i] == this->node_hash_)
      return;  // on est deja passe dans ce chemin, on ne relaie pas deux fois
  }

  std::vector<uint8_t> packet;
  packet.push_back(header);
  packet.push_back(static_cast<uint8_t>(path_len + 1));
  packet.insert(packet.end(), path, path + path_len);
  packet.push_back(this->node_hash_);
  packet.insert(packet.end(), payload, payload + payload_len);

  ESP_LOGV(TAG, "Relais flood (path_len=%u -> %u)", path_len, path_len + 1);
  this->transmit_raw_packet(packet);
}

void MeshCore::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) {
  if (packet.size() < 2) {
    ESP_LOGV(TAG, "Paquet trop court, ignore");
    return;
  }

  uint8_t header = packet[0];
  uint8_t route_type = header & PH_ROUTE_MASK;
  uint8_t payload_type = (header >> PH_TYPE_SHIFT) & PH_TYPE_MASK;

  // Toujours visible en logger:level:debug, meme si le paquet est ensuite
  // rejete (canal inconnu, doublon, type non gere...) : ca permet de
  // distinguer "rien n'arrive en RF" de "quelque chose arrive mais est
  // rejete plus loin".
  ESP_LOGD(TAG, "Paquet RX brut: %u octets, route=%u, payload_type=0x%02x, rssi=%.1f, snr=%.1f", packet.size(),
           route_type, payload_type, rssi, snr);

  size_t idx = 1;

  if (route_type == ROUTE_TYPE_TRANSPORT_FLOOD || route_type == ROUTE_TYPE_TRANSPORT_DIRECT) {
    // Codes de transport (4 octets) non geres dans cette v1 - voir
    // NOTES_PROTOCOLE.md. On abandonne plutot que de mal interpreter le
    // paquet.
    ESP_LOGV(TAG, "Paquet avec codes de transport, non supporte, ignore");
    return;
  }

  if (idx >= packet.size())
    return;
  uint8_t path_len = packet[idx++];
  if (path_len > MAX_PATH_SIZE || idx + path_len > packet.size())
    return;
  const uint8_t *path = packet.data() + idx;
  idx += path_len;

  const uint8_t *payload = packet.data() + idx;
  size_t payload_len = packet.size() - idx;

  uint32_t phash = MeshCore::hash_payload(payload_type, payload, payload_len);
  if (this->seen_hashes_.count(phash) != 0) {
    ESP_LOGV(TAG, "Paquet deja vu, ignore (dedup)");
    return;
  }
  this->seen_hashes_.insert(phash);
  // NOTE: seen_hashes_ n'est jamais purge dans cette v1 (pas d'horodatage
  // d'expiration comme le FLOOD_EXPIRE_TIME du composant meshtastic) - a
  // ajouter si le noeud tourne tres longtemps sans redemarrer, voir
  // NOTES_PROTOCOLE.md.

  if (payload_type == PAYLOAD_TYPE_GRP_TXT) {
    this->handle_group_text(payload, payload_len, rssi, snr);
  } else {
    ESP_LOGV(TAG, "Type de payload 0x%02x non gere par cette v1, ignore", payload_type);
  }

  if (route_type == ROUTE_TYPE_FLOOD) {
    this->maybe_repeat_flood(header, path, path_len, payload, payload_len);
  }
}

}  // namespace meshcore
}  // namespace esphome
