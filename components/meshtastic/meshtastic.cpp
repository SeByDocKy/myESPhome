#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"

#include "meshtastic.h"

#include <pb_decode.h>
#include <pb_encode.h>
#include <esp_wifi.h>

#if !defined(USE_HOST)
#define MBEDTLS_AES_ALT
#endif
#include "mbedtls/aes.h"
#define MBEDTLS_CIPHER_MODE_CTR
#include "mbedtls/sha256.h"
#include "mbedtls/ccm.h"

#if !defined(MESHTASTIC_EXCLUDE_PKI)
#include <sodium.h>
#endif

namespace esphome {
namespace meshtastic {

static const char *TAG = "meshtastic.component";

static uint8_t xor_hash(const uint8_t *p, size_t len)
{
    uint8_t code = 0;
    for (size_t i = 0; i < len; i++)
        code ^= p[i];
    return code;
}

void Channel::generate_hash()
{
    psk_t k = get_psk();
    if (!k.empty()) {
        const char *name = get_name().c_str();
        uint8_t h = xor_hash((const uint8_t *)name, strlen(name));

        h ^= xor_hash(k.data(), k.size());
        set_hash(h);
        ESP_LOGD(TAG, "Channel '%s' hash generated: 0x%02x", name, h);
    }
}

static void init_nonce(uint8_t* nonce, uint32_t from_node, uint64_t packet_id, uint32_t extra_nonce = 0) {
  memset(nonce, 0, 16);
  memcpy(nonce, &packet_id, sizeof(uint64_t));
  memcpy(nonce + sizeof(uint64_t), &from_node, sizeof(uint32_t));
  if (extra_nonce)
    memcpy(nonce + sizeof(uint32_t), &extra_nonce, sizeof(uint32_t));
}

static bool aes_decrypt_meshtastic_payload(const uint8_t* key, uint16_t keySize, uint32_t packet_id, uint32_t from_node, const uint8_t* encrypted_in, uint8_t* decrypted_out, size_t len) {
  mbedtls_aes_context aes_ctx;
  mbedtls_aes_init(&aes_ctx);

  int ret = mbedtls_aes_setkey_enc(&aes_ctx, key, keySize);
  if (ret != 0) {
    ESP_LOGE(TAG, "mbedtls_aes_setkey_enc failed with error: -0x%04x", -ret);
    mbedtls_aes_free(&aes_ctx);
    return false;
  }
  uint8_t nonce[16];
  uint8_t stream_block[16];
  size_t nc_off = 0;
  init_nonce(nonce, from_node, packet_id, 0);

  ret = mbedtls_aes_crypt_ctr(&aes_ctx, len, &nc_off, nonce, stream_block, encrypted_in, decrypted_out);
  if (ret != 0) {
      ESP_LOGE(TAG, "mbedtls_aes_crypt_ctr failed with error: -0x%04x", -ret);
      mbedtls_aes_free(&aes_ctx);
      return false;
  }
  mbedtls_aes_free(&aes_ctx);
  return true;
}

#if !defined(MESHTASTIC_EXCLUDE_PKI)

void hash(uint8_t* data, size_t len) {
  mbedtls_sha256_context hash_ctx_;
  mbedtls_sha256_init(&hash_ctx_);
  mbedtls_sha256_starts(&hash_ctx_, 0);  // 0 = SHA256, not SHA224
  mbedtls_sha256_update(&hash_ctx_, data, len);
  mbedtls_sha256_finish(&hash_ctx_, data);
  mbedtls_sha256_free(&hash_ctx_);
}

bool Meshtastic::init_shared_key(uint8_t* shared_key, uint8_t* pubKey) {
    uint8_t local_priv[32];
    memcpy(local_priv, owner_node_->private_key_.data(), owner_node_->private_key_.size());
    if (crypto_scalarmult(shared_key, local_priv, pubKey) != 0) {
        ESP_LOGE("MT_CRYPTO", "crypto_scalarmult failed - weak or invalid key detected!");
        return false;
    }
    return true;
}

bool Meshtastic::encrypt_curve25519(uint32_t toNode, uint32_t fromNode, uint8_t* remotePublic,
                                    uint64_t packetNum, size_t numBytes, const uint8_t* bytes,
                                    uint8_t* bytesOut) {
    uint8_t nonce[16];
    uint8_t shared_key[32];
    uint8_t* auth;

    long extra_nonce_tmp = random_uint32();
    auth = bytesOut + numBytes;
    memcpy((uint8_t*)(auth + 8), &extra_nonce_tmp, sizeof(uint32_t));

    if (!init_shared_key(shared_key, remotePublic)) {
        return false;
    }

    hash(shared_key, 32);
    init_nonce(nonce, fromNode, packetNum, extra_nonce_tmp);

    ESP_LOGD("MT_CRYPTO", "Encrypting with Curve25519 packet %llx extraNonce %lx", packetNum, extra_nonce_tmp);

    mbedtls_ccm_context ccm_ctx;
    mbedtls_ccm_init(&ccm_ctx);
    int ret = mbedtls_ccm_setkey(&ccm_ctx, MBEDTLS_CIPHER_ID_AES, shared_key, 256);
    if (ret != 0) {
        ESP_LOGE("MT_CRYPTO", "mbedtls_ccm_setkey failed: %d", ret);
        mbedtls_ccm_free(&ccm_ctx);
        return false;
    }
    ret = mbedtls_ccm_encrypt_and_tag(&ccm_ctx, numBytes, nonce, 13, nullptr, 0, bytes, bytesOut, auth, 8);
    mbedtls_ccm_free(&ccm_ctx);
    if (ret != 0) {
        ESP_LOGE("MT_CRYPTO", "mbedtls_ccm_encrypt_and_tag failed: %d", ret);
        return false;
    }
    memcpy((uint8_t*)(auth + 8), &extra_nonce_tmp, sizeof(uint32_t));
    return true;
}

bool Meshtastic::decrypt_curve25519(uint32_t fromNode, uint8_t* remotePublic, uint64_t packetNum,
                                    size_t numBytes, const uint8_t* bytes, uint8_t* bytesOut) {
    uint8_t nonce[16];
    uint8_t shared_key[32];

    const uint8_t* auth = bytes + numBytes - 12;
    uint32_t extra_nonce;
    memcpy(&extra_nonce, auth + 8, sizeof(uint32_t));

    ESP_LOGD("MT_CRYPTO", "Decrypting with Curve25519 packet %llu extraNonce %u", packetNum, extra_nonce);

    if (!init_shared_key(shared_key, remotePublic)) {
        ESP_LOGE("MT_CRYPTO", "Failed to calculate shared key");
        return false;
    }
    hash(shared_key, 32);
    init_nonce(nonce, fromNode, packetNum, extra_nonce);

    mbedtls_ccm_context ccm_ctx;
    mbedtls_ccm_init(&ccm_ctx);
    int ret = mbedtls_ccm_setkey(&ccm_ctx, MBEDTLS_CIPHER_ID_AES, shared_key, 256);
    if (ret != 0) {
        ESP_LOGE("MT_CRYPTO", "mbedtls_ccm_setkey failed: %d", ret);
        mbedtls_ccm_free(&ccm_ctx);
        return false;
    }
    size_t ciphertext_len = numBytes - 12;
    ret = mbedtls_ccm_auth_decrypt(&ccm_ctx, ciphertext_len, nonce, 13, nullptr, 0, bytes, bytesOut, auth, 8);
    mbedtls_ccm_free(&ccm_ctx);
    if (ret != 0) {
        ESP_LOGE("MT_CRYPTO", "mbedtls_ccm_auth_decrypt failed: %d (authentication failed or corrupted data)", ret);
        return false;
    }
    return true;
}
#endif

bool Meshtastic::pb_decode_from_bytes(const uint8_t* srcbuf, size_t srcbufsize, const pb_msgdesc_t* fields, void* dest_struct) {
    pb_istream_t stream = pb_istream_from_buffer(srcbuf, srcbufsize);
    if (!pb_decode(&stream, fields, dest_struct)) {
        ESP_LOGW(TAG, "Can't decode protobuf reason='%s', pb_msgdesc %p", PB_GET_ERROR(&stream), fields);
        return false;
    } else {
        return true;
    }
}

size_t Meshtastic::pb_encode_to_bytes(uint8_t* destbuf, size_t destbufsize, const pb_msgdesc_t* fields, const void* src_struct) {
    pb_ostream_t stream = pb_ostream_from_buffer(destbuf, destbufsize);
    if (!pb_encode(&stream, fields, src_struct)) {
        ESP_LOGE(TAG, "Panic: can't encode protobuf reason='%s'", PB_GET_ERROR(&stream));
        return 0;
    } else {
        return stream.bytes_written;
    }
}

void PacketHistory::clearExpiredRecentPackets() {
  uint32_t now = millis();

  ESP_LOGD(TAG, "recentPackets size=%ld\n", recentPackets.size());

  for (auto it = recentPackets.begin(); it != recentPackets.end();) {
    if ((now - it->rxTimeMsec) >= FLOOD_EXPIRE_TIME) {
      it = recentPackets.erase(it); // erase returns iterator pointing to element immediately following the one erased
    } else {
      ++it;
    }
  }
  ESP_LOGD(TAG, "recentPackets size=%ld (after clearing expired packets)\n", recentPackets.size());
}

bool PacketHistory::wasSeenRecently(const meshtastic_MeshPacket &p, bool withUpdate) {
  if (p.id == 0) {
      ESP_LOGD(TAG, "Ignoring message with zero id\n");
      return false;
  }
  uint32_t now = millis();
  PacketRecord r;
  r.id = p.id;
  r.sender = p.from;
  r.rxTimeMsec = now;

  auto found = recentPackets.find(r);
  bool seen_recently = (found != recentPackets.end());
  if (seen_recently && (now - found->rxTimeMsec) >= FLOOD_EXPIRE_TIME) {
    recentPackets.erase(found);
    found = recentPackets.end();
    seen_recently = false;
  }
  if (seen_recently) {
      ESP_LOGD(TAG, "Found existing packet record for fr=0x%x,to=0x%x,id=0x%x\n", p.from, p.to, p.id);
  }

  if (withUpdate) {
    if (found != recentPackets.end()) {
      recentPackets.erase(found);
    }
    recentPackets.insert(r);
  }
  if (recentPackets.size() > (MAX_NUM_NODES * 0.9)) {
    clearExpiredRecentPackets();
  }
  return seen_recently;
}

void Meshtastic::setup() {
  uint8_t mac[6];
  char mac_address[18];
  get_mac_address_raw(mac);
  node_num_ = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
  format_mac_addr_upper(mac, mac_address);
  ESP_LOGI(TAG, "Meshtastic %x MAC address: %s", node_num_, mac_address);

  snprintf(owner.id, sizeof(owner.id), "!%08x", node_num_);
  if(owner.hw_model == meshtastic_HardwareModel_UNSET)
    owner.hw_model = meshtastic_HardwareModel_DIY_V1;
  owner.role = meshtastic_Config_DeviceConfig_Role_CLIENT_MUTE;
  memcpy(owner.macaddr, mac, sizeof(owner.macaddr));
  if(owner.long_name[0] == '\0')
    snprintf(owner.long_name, sizeof(owner.long_name), "ESPHome %04X", node_num_ & 0xFFFF);
  if(owner.short_name[0] == '\0')
    snprintf(owner.short_name, sizeof(owner.short_name), "%04X", node_num_ & 0xFFFF);

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  if(this->use_udp_) {
    esp_wifi_set_ps(WIFI_PS_NONE);

    //ip_addr_ismulticast(&ip_addr_);

    for (const auto &address : this->addresses_) {
      struct sockaddr saddr {};
      socket::set_sockaddr(&saddr, sizeof(saddr), address, this->broadcast_port_);
      this->sockaddrs_.push_back(saddr);
    }
    if (this->multicast_address_.has_value()) {
      char addr_buf[network::IP_ADDRESS_BUFFER_SIZE];
      this->multicast_address_.value().str_to(addr_buf);
      struct sockaddr saddr {};
      socket::set_sockaddr(&saddr, sizeof(saddr), addr_buf, UDP_MULTICAST_DEFAUL_PORT);
      this->sockaddrs_.push_back(saddr);
      ESP_LOGI(TAG, "IP multicast: %s", addr_buf);
    }
    if (this->sockaddrs_.size()) {
      this->broadcast_socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
      if (this->broadcast_socket_ == nullptr) {
        this->status_set_error(LOG_STR("Could not create socket"));
        this->mark_failed();
        return;
      }
      int enable = 1;
      auto err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
      if (err != 0) {
      this->status_set_warning("Socket unable to set reuseaddr");
        // we can still continue
      }
      err = this->broadcast_socket_->setsockopt(SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int));
      if (err != 0) {
        this->status_set_warning("Socket unable to set broadcast");
      }
    }

    this->listen_socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (this->listen_socket_ == nullptr) {
      this->status_set_error(LOG_STR("Could not create socket"));
      this->mark_failed();
      return;
    }
    auto err = this->listen_socket_->setblocking(false);
    if (err < 0) {
      ESP_LOGE(TAG, "Unable to set nonblocking: errno %d", errno);
      this->status_set_error(LOG_STR("Unable to set nonblocking"));
      this->mark_failed();
      return;
    }
    int enable = 1;
    err = this->listen_socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    if (err != 0) {
      this->status_set_warning("Socket unable to set reuseaddr");
      // we can still continue
    }
    struct sockaddr_in server {};

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = ESPHOME_INADDR_ANY;
    server.sin_port = htons(this->listen_port_);
    if (this->multicast_address_.has_value()) {
      char addr_buf[network::IP_ADDRESS_BUFFER_SIZE];
      this->multicast_address_.value().str_to(addr_buf);
      struct ip_mreq imreq = {};
      imreq.imr_interface.s_addr = ESPHOME_INADDR_ANY;
      inet_aton(addr_buf, &imreq.imr_multiaddr);
      server.sin_addr.s_addr = imreq.imr_multiaddr.s_addr;
      ESP_LOGD(TAG, "Join multicast %s:%d", addr_buf, UDP_MULTICAST_DEFAUL_PORT);
      err = this->listen_socket_->setsockopt(IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(imreq));
      if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
        this->status_set_error(LOG_STR("Failed to set IP_ADD_MEMBERSHIP"));
        this->mark_failed();
        return;
      }
    }
    err = this->listen_socket_->bind((struct sockaddr *) &server, sizeof(server));
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      this->status_set_error(LOG_STR("Unable to bind socket"));
      this->mark_failed();
      return;
    }
  }
#endif
  if(node_broadcast_interval_ > 0) {
    node_broadcast_interval_ = std::max<uint32_t>(node_broadcast_interval_, (60*60*1000));
    this->set_interval("Owner NodeInfo", node_broadcast_interval_, [this] { this->send_our_node_info(NODENUM_BROADCAST, 0, meshtastic_MeshPacket_Priority_BACKGROUND); });
  }
  if(position_broadcast_interval_ > 0 && owner_position.has_latitude_i && owner_position.has_longitude_i) {
    position_broadcast_interval_ = std::max<uint32_t>(position_broadcast_interval_, (15*60*1000));
    this->set_interval("Owner Position", position_broadcast_interval_, [this] { this->send_our_node_position(NODENUM_BROADCAST, 0, meshtastic_MeshPacket_Priority_BACKGROUND); });
  }
}

void Meshtastic::loop() {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  if(this->use_udp_) {
    auto buf = std::vector<uint8_t>(meshtastic_MeshPacket_size);
    sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    for (;;) {
      auto len = this->listen_socket_->recvfrom(buf.data(), buf.size(), (struct sockaddr *) &addr, &addrlen);
      if (len <= 0)
        break;
      buf.resize(len);
      ESP_LOGV(TAG, "Received packet of length %zu", len);
      this->on_udp_packet(buf, &addr);
    }
  }
#endif
}

void Meshtastic::dump_config(){
    ESP_LOGCONFIG(TAG, "Meshtastic");
    ESP_LOGCONFIG(TAG, "  Node Broadcast Interval: %d", node_broadcast_interval_);
    ESP_LOGCONFIG(TAG, "  Position Broadcast Interval: %d", position_broadcast_interval_);
    ESP_LOGCONFIG(TAG, "  Node Num: %08X", node_num_);
    ESP_LOGCONFIG(TAG, "  Owner ID: %s", owner.id);
    ESP_LOGCONFIG(TAG, "  Owner Long Name: %s", owner.long_name);
    ESP_LOGCONFIG(TAG, "  Owner Short Name: %s", owner.short_name);
    ESP_LOGCONFIG(TAG, "  Owner HW Model: %d", owner.hw_model);
    ESP_LOGCONFIG(TAG, "  Owner Role: %d", owner.role);
    if(owner.public_key.size)
      ESP_LOGCONFIG(TAG, "  Owner public key: %s", format_hex_pretty(owner.public_key.bytes, owner.public_key.size).c_str());
    ESP_LOGCONFIG(TAG, "  Use UDP: %s", this->use_udp_ ? "Yes" : "No");
    if(this->use_udp_) {
      if (this->multicast_address_.has_value()) {
        char addr_buf[network::IP_ADDRESS_BUFFER_SIZE];
        this->multicast_address_.value().str_to(addr_buf);
        ESP_LOGCONFIG(TAG, "    Multicast Address: %s", addr_buf);
      }
      for (const auto &address : this->addresses_)
        ESP_LOGCONFIG(TAG, "    IP: %s", address.c_str());
      ESP_LOGCONFIG(TAG, "    Bridge Mode: %s", this->bridge_mode_ ? "Yes" : "No");
    }
    ESP_LOGCONFIG(TAG, "  Hop Limit: %d", this->hop_limit_);
    ESP_LOGCONFIG(TAG, "  Ignore MQTT: %s", this->ignore_mqtt_ ? "Yes" : "No");
    ESP_LOGCONFIG(TAG, "  OK to MQTT: %s", this->ok_to_mqtt_ ? "Yes" : "No");
    if(owner_position.has_latitude_i && owner_position.has_longitude_i)
      ESP_LOGCONFIG(TAG, "  Owner Position: lat %f lon %f alt %d", this->owner_position.latitude_i * 1e-7, this->owner_position.longitude_i * 1e-7, this->owner_position.altitude);

    ESP_LOGCONFIG(TAG, "  Channels:");
    for (auto &channel : channels_)
      ESP_LOGCONFIG(TAG, "    Name: %s Hash: %02X", channel.get_name().c_str(), channel.get_hash());
    ESP_LOGCONFIG(TAG, "  Nodes:");
    for (auto &node : nodes_)
      ESP_LOGCONFIG(TAG, "    Name: %s (%s) Number: %08X", node->name_.c_str(), node->name_short_.c_str(), node->node_num_);

}

#ifdef USE_SX126X
  void Meshtastic::set_lora(sx126x::SX126x* sx) {
    this->sx126x_ = sx;
    this->sx126x_->register_listener(this);
  };
#endif
#ifdef USE_SX127X
  void Meshtastic::set_lora(sx127x::SX127x* sx) {
    this->sx127x_ = sx;
    this->sx127x_->register_listener(this);
  };
#endif

#define PACKET_FLAGS_HOP_LIMIT_MASK 0x07
#define PACKET_FLAGS_WANT_ACK_MASK 0x08
#define PACKET_FLAGS_VIA_MQTT_MASK 0x10
#define PACKET_FLAGS_HOP_START_MASK 0xE0
#define PACKET_FLAGS_HOP_START_SHIFT 5

bool Meshtastic::transmit_radio_packet(meshtastic_MeshPacket *mp) {
//  ESP_LOGD(TAG, "transmit_radio_packet %s", format_hex_pretty(packet, ',', true).c_str());
  RadioBuffer radio_buffer;
  memset(&radio_buffer, 0, sizeof(RadioBuffer));
  radio_buffer.header.from = mp->from;
  radio_buffer.header.to = mp->to;
  radio_buffer.header.id = mp->id;
  radio_buffer.header.channel = mp->channel;
  radio_buffer.header.next_hop = mp->next_hop;
  radio_buffer.header.relay_node = mp->relay_node;
  radio_buffer.header.flags = mp->hop_limit | (mp->want_ack ? PACKET_FLAGS_WANT_ACK_MASK : 0) | (mp->via_mqtt ? PACKET_FLAGS_VIA_MQTT_MASK : 0);
  radio_buffer.header.flags |= (mp->hop_start << PACKET_FLAGS_HOP_START_SHIFT) & PACKET_FLAGS_HOP_START_MASK;

  if (mp->encrypted.size > sizeof(radio_buffer.payload)) {
    ESP_LOGE(TAG, "Encrypted payload too large: %u > %u", mp->encrypted.size, sizeof(radio_buffer.payload));
    return false;
  }
  memcpy(radio_buffer.payload, mp->encrypted.bytes, mp->encrypted.size);
  std::vector<uint8_t> packet(reinterpret_cast<const uint8_t*>(&radio_buffer), reinterpret_cast<const uint8_t*>(&radio_buffer) + sizeof(PacketHeader) + mp->encrypted.size);

#ifdef USE_SX126X
  if(this->sx126x_) {
    ESP_LOGD(TAG, "transmit packet SX126X");
    return this->sx126x_->transmit_packet(packet) == sx126x::SX126xError::NONE;
  }
#endif
#ifdef USE_SX127X
  if(this->sx127x_) {
    ESP_LOGD(TAG, "transmit packet SX127X");
    return this->sx127x_->transmit_packet(packet) == sx127x::SX127xError::NONE;
  }
#endif
  if(!this->use_udp_)
  {
    ESP_LOGE(TAG, "No LoRa radio configured!");
    return false;
  }
  return true;
}

bool Meshtastic::transmit_udp_packet(meshtastic_MeshPacket *mp, int ntries) {
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
    uint8_t data[meshtastic_MeshPacket_size];
    size_t size = pb_encode_to_bytes(data, sizeof(data), &meshtastic_MeshPacket_msg, mp);
    ESP_LOGD(TAG, "transmit_udp_packet(%d) id=%08X to=%08X channel %x %s", ntries, mp->id, mp->to, mp->channel, format_hex_pretty(data, size, ',', true).c_str());

//    struct sockaddr saddr {};
//    socket::set_sockaddr(&saddr, sizeof(saddr), this->multicast_address_.value().str().c_str(), UDP_MULTICAST_DEFAUL_PORT);
    for (const auto &saddr : this->sockaddrs_) {
      auto result = this->broadcast_socket_->sendto(data, size, 0, &saddr, sizeof(saddr));

      const sockaddr_in *sin = reinterpret_cast<const sockaddr_in*>(&saddr);
      char ip_str[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(sin->sin_addr), ip_str, INET_ADDRSTRLEN);
      ESP_LOGD(TAG, "sendto %s result %d", ip_str, result);

      if (result < 0) {
        ESP_LOGW(TAG, "sendto() error %d", errno);
        return false;
      }
    }
#endif
  return true;
}

#define ID_COUNTER_MASK (UINT32_MAX >> 22)
PacketId Meshtastic::generatePacketId() {
    static uint32_t rolling_packet_id; // Note: trying to keep this in noinit didn't help for working across reboots
    static bool did_init = false;

    if (!did_init) {
        did_init = true;

        // pick a random initial sequence number at boot (to prevent repeated reboots always starting at 0)
        rolling_packet_id = random_uint32();
        ESP_LOGD(TAG, "Initial packet id %u", rolling_packet_id);
    }

    rolling_packet_id++;

    rolling_packet_id &= ID_COUNTER_MASK;                                    // Mask out the top 22 bits
    PacketId id = rolling_packet_id | random_uint32() << 10; // top 22 bits
    ESP_LOGD(TAG, "Partially randomized packet id %x", id);
    return id;
}

Channel* Meshtastic::get_channel_by_hash(uint8_t hash) {
  auto channel = std::find_if(channels_.begin(), channels_.end(), [&hash](const Channel& ch) {
    return ch.get_hash() == hash;
  });

  if (channel == channels_.end()) {
    return nullptr;
  }
  return &(*channel);
}

Channel* Meshtastic::get_channel_by_name(const std::string &name) {
  auto channel = std::find_if(channels_.begin(), channels_.end(), [&name](const Channel& ch) {
    return str_equals_case_insensitive(ch.get_name(), name);
  });

  if (channel == channels_.end()) {
    return nullptr;
  }
  return &(*channel);
}

Node* Meshtastic::get_node_by_num(const NodeNum num) {
  auto node = std::find_if(nodes_.begin(), nodes_.end(), [&num](const std::unique_ptr<Node>& n) {
    return n->node_num_ == num;
  });

  if (node == nodes_.end()) {
    return nullptr;
  }
  return node->get();
}

bool Meshtastic::send_text_message(NodeNum to, const std::string &channel_name, const std::string &msg) {

  ESP_LOGI(TAG, "send_text_message to=%08X channel='%s' '%s'", to, channel_name.c_str(), msg.c_str());

  meshtastic_MeshPacket mp;
  memset(&mp, 0, sizeof(mp));
  mp.to = to;
  mp.from = node_num_;
  mp.id = generatePacketId();
//  mp.priority = meshtastic_MeshPacket_Priority_RELIABLE;
  mp.priority = meshtastic_MeshPacket_Priority_ALERT;
  mp.hop_limit = this->hop_limit_;
  mp.hop_start = 0;
  mp.want_ack = false;
  mp.via_mqtt = false;
  Channel *channel;
  if(channel_name.empty()) {
    channel = &channels_.front();
  } else {
    channel = get_channel_by_name(channel_name);
    if(channel == nullptr) {
      ESP_LOGW(TAG, "No channel found '%s'", channel_name.c_str());
      return false;
    }
  }
  if (channel == nullptr) {
    ESP_LOGW(TAG, "No channels configured, cannot transmit packet");
    return false;
  }
  mp.channel = channel->get_hash();
  if(to && to != NODENUM_BROADCAST) {
    mp.channel = 0; // direct messages are sent on channel 0
    mp.want_ack = true;
  }
  mp.decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
  mp.decoded.payload.size = msg.length();
  std::memcpy(mp.decoded.payload.bytes, msg.c_str(), msg.length());

  return transmit_packet_encrypt(mp);
}

//packet transport
bool Meshtastic::transmit_packet(const std::vector<uint8_t> &packet, uint8_t hop_limit) {
  ESP_LOGD(TAG, "transmit_packet protocol_packet hop_limit=%d %s", hop_limit, format_hex_pretty(packet, ',', true).c_str());
  meshtastic_MeshPacket mp;
  memset(&mp, 0, sizeof(mp));
  if(channels_.empty()) {
    ESP_LOGW(TAG, "No channels configured, cannot transmit packet");
    return false;
  }
  mp.channel = channels_.front().get_hash();
  mp.to = NODENUM_BROADCAST;
  mp.from = node_num_;
  mp.id = generatePacketId();
  mp.priority = meshtastic_MeshPacket_Priority_ALERT;
  mp.hop_limit = hop_limit;
  mp.hop_start = 0;
  mp.want_ack = false;
  mp.via_mqtt = false;

  mp.decoded.portnum = meshtastic_PortNum_DETECTION_SENSOR_APP;
  mp.decoded.payload.size = packet.size();
  std::memcpy(mp.decoded.payload.bytes, packet.data(), packet.size());
  return transmit_packet_encrypt(mp);
}

#define BITFIELD_WANT_RESPONSE_SHIFT 1
#define BITFIELD_OK_TO_MQTT_SHIFT 0
#define BITFIELD_WANT_RESPONSE_MASK (1 << BITFIELD_WANT_RESPONSE_SHIFT)
#define BITFIELD_OK_TO_MQTT_MASK (1 << BITFIELD_OK_TO_MQTT_SHIFT)

bool Meshtastic::transmit_packet_encrypt(meshtastic_MeshPacket &mp, bool want_response) {

  if (mp.to == NODENUM_BROADCAST)
    mp.want_ack = false;

  if (mp.from == this->node_num_)
    mp.hop_start = mp.hop_limit;

  ESP_LOGI(TAG, "transmit_packet_encrypt id=%x fr=%x to=%x ch=0x%x next_hop=%x relay_node=%x hop=%d/%d mqtt=%d", mp.id, mp.from, mp.to, mp.channel, mp.next_hop, mp.relay_node, mp.hop_start, mp.hop_limit, mp.via_mqtt);

  mp.decoded.has_bitfield = true;
  mp.decoded.bitfield |= (this->ok_to_mqtt_ << BITFIELD_OK_TO_MQTT_SHIFT);
  mp.decoded.bitfield |= (want_response << BITFIELD_WANT_RESPONSE_SHIFT);

  uint8_t bytes[MAX_LORA_PAYLOAD_LEN + 1];
  size_t numbytes = pb_encode_to_bytes(bytes, sizeof(bytes), meshtastic_Data_fields, &mp.decoded);

  if(mp.to && mp.to != this->node_num_ && mp.to != NODENUM_BROADCAST && mp.channel == 0) {
    Node* dest_node = get_node_by_num(mp.to);
#if !defined(MESHTASTIC_EXCLUDE_PKI)
    if(dest_node == nullptr) {
      ESP_LOGW(TAG, "No node found for num 0x%08x to encrypt with Curve25519", mp.to);
      return false;
    }
    encrypt_curve25519(mp.to, mp.from, dest_node->public_key_.data(), mp.id, numbytes, bytes, mp.encrypted.bytes);
    numbytes += 12; // auth size
#else
    return false;
#endif
  } else {
    auto *channel = get_channel_by_hash(mp.channel);
    if(channel == nullptr) {
      ESP_LOGW(TAG, "No channel found for hash 0x%02x", mp.channel);
      return false;
    }
    psk_t key = channel->get_psk();
    if(key.empty()) {
      ESP_LOGD(TAG, "data unencrypted payload %s", format_hex_pretty(bytes, numbytes, ',', true).c_str());
      memcpy(mp.encrypted.bytes, bytes, numbytes);
    }
    else
    {
      aes_decrypt_meshtastic_payload(key.data(), key.size() * 8, mp.id, mp.from, bytes, mp.encrypted.bytes, numbytes);
      ESP_LOGD(TAG, "encrypted payload %s", format_hex_pretty(mp.encrypted.bytes, numbytes, ',', true).c_str());
    }
  }
  mp.encrypted.size = numbytes;
  mp.which_payload_variant = meshtastic_MeshPacket_encrypted_tag;

  if (!transmit_radio_packet(&mp)) {
    ESP_LOGW(TAG, "transmit_radio_packet timeout");
  }

  if (this->use_udp_) {
    transmit_udp_packet(&mp);
  }
  return true;
}

void Meshtastic::send_our_node_info(NodeNum to, uint8_t channel, meshtastic_MeshPacket_Priority priority) {
  ESP_LOGI(TAG, "send_our_node_info");
  meshtastic_MeshPacket mp;
  memset(&mp, 0, sizeof(mp));
  if(channels_.empty()) {
    ESP_LOGW(TAG, "No channels configured, cannot transmit packet");
    return;
  }
  if(!channel) {
    mp.channel = channels_.front().get_hash();
  } else {
    mp.channel = channel;
  }
  mp.to = to;
  mp.from = node_num_;
  mp.id = generatePacketId();
  mp.priority = priority;
  mp.hop_limit = this->hop_limit_;
  mp.hop_start = 0;
  mp.want_ack = false;
  mp.via_mqtt = false;

  mp.decoded.portnum = meshtastic_PortNum_NODEINFO_APP;
  mp.decoded.payload.size = pb_encode_to_bytes(mp.decoded.payload.bytes, sizeof(mp.decoded.payload.bytes), &meshtastic_User_msg, &owner);

  transmit_packet_encrypt(mp);
}

void Meshtastic::send_our_node_position(NodeNum to, uint8_t channel, meshtastic_MeshPacket_Priority priority) {
  ESP_LOGI(TAG, "send_owner_position");
  if(owner_position.has_latitude_i == false || owner_position.has_longitude_i == false) {
    ESP_LOGW(TAG, "No owner position set, cannot transmit position packet");
    return;
  }
  meshtastic_MeshPacket mp;
  memset(&mp, 0, sizeof(mp));
  if(channels_.empty()) {
    ESP_LOGW(TAG, "No channels configured, cannot transmit packet");
    return;
  }
  if(!channel) {
    mp.channel = channels_.front().get_hash();
  } else {
    mp.channel = channel;
  }
  mp.to = to;
  mp.from = node_num_;
  mp.id = generatePacketId();
  mp.priority = priority;
  mp.hop_limit = this->hop_limit_;
  mp.hop_start = 0;
  mp.want_ack = false;
  mp.via_mqtt = false;

  owner_position.seq_number++;
  mp.decoded.portnum = meshtastic_PortNum_POSITION_APP;
  mp.decoded.payload.size = pb_encode_to_bytes(mp.decoded.payload.bytes, sizeof(mp.decoded.payload.bytes), &meshtastic_Position_msg, &owner_position);

  transmit_packet_encrypt(mp);
}

uint8_t Meshtastic::get_hop_limit4response(uint8_t hop_start, uint8_t hop_limit)
{
  if (hop_start != 0) {
    // Hops used by the request. If somebody in between running modified firmware modified it, ignore it
    uint8_t hops_used = hop_start < hop_limit ? this->hop_limit_ : hop_start - hop_limit;
    if (hops_used > this->hop_limit_) {
      return hops_used;
    } else if ((uint8_t)(hops_used + 2) < this->hop_limit_) {
      return hops_used + 2; // Use only the amount of hops needed with some margin as the way back may be different
    }
  }
  return this->hop_limit_;
}

void Meshtastic::send_ack(NodeNum to, PacketId id_from, uint8_t channel, meshtastic_Routing_Error err) {
  ESP_LOGD(TAG, "send_ack to=%08X for id=%x err=%d", to, id_from, err);
  meshtastic_MeshPacket mp;
  memset(&mp, 0, sizeof(mp));
  mp.to = to;
  mp.from = node_num_;
  mp.id = generatePacketId();
  mp.channel = channel;
  mp.priority = meshtastic_MeshPacket_Priority_ACK;
  mp.hop_limit = this->hop_limit_;
  mp.hop_start = 0;
  mp.want_ack = false;
  mp.via_mqtt = false;

  meshtastic_Routing c = meshtastic_Routing_init_default;
  c.error_reason = err;
  c.which_variant = meshtastic_Routing_error_reason_tag;

  mp.decoded.portnum = meshtastic_PortNum_ROUTING_APP;
  mp.decoded.request_id = id_from;
  mp.decoded.payload.size = pb_encode_to_bytes(mp.decoded.payload.bytes, sizeof(mp.decoded.payload.bytes), &meshtastic_Routing_msg, &c);

  transmit_packet_encrypt(mp);
}

#define ROUTE_SIZE sizeof(((meshtastic_RouteDiscovery *)0)->route) / sizeof(((meshtastic_RouteDiscovery *)0)->route[0])

void Meshtastic::handle_traceroute(meshtastic_MeshPacket *p, PacketId request_id, meshtastic_RouteDiscovery *r) {
#if !defined(MESHTASTIC_EXCLUDE_TRACEROUTE)

#if 0
  for (uint8_t i = 0; i < r->route_count; i++) {
    ESP_LOGI(TAG, "route %i 0x%x", i, r->route[i]);
  }
  for (uint8_t i = 0; i < r->snr_towards_count; i++) {
    ESP_LOGI(TAG, "snr %i %.2fdB", i, (float) r->snr_towards[i] / 4);
  }
  for (uint8_t i = 0; i < r->route_back_count; i++) {
    ESP_LOGI(TAG, "route back %i 0x%x", i, r->route_back[i]);
  }
  for (uint8_t i = 0; i < r->snr_back_count; i++) {
    ESP_LOGI(TAG, "snr back %i %.2fdB", i, (float) r->snr_back[i] / 4);
  }
#endif
  if(p->to == this->node_num_) {
    // Only insert unknown hops if hop_start is valid
    if (p->hop_start != 0 && p->hop_limit <= p->hop_start) {
      uint8_t hops_taken = p->hop_start - p->hop_limit;
      int8_t diff = hops_taken - r->route_count;
      for (uint8_t i = 0; i < diff; i++) {
        if (r->route_count < ROUTE_SIZE) {
          r->route[r->route_count] = NODENUM_BROADCAST; // This will represent an unknown hop
          r->route_count += 1;
        }
      }
      // Add unknown SNR values if necessary
      diff = r->route_count - r->snr_towards_count;
      for (uint8_t i = 0; i < diff; i++) {
        if (r->snr_towards_count < ROUTE_SIZE) {
          r->snr_towards[r->snr_towards_count] = INT8_MIN; // This will represent an unknown SNR
          r->snr_towards_count += 1;
        }
      }
    }
  }

  for (uint8_t i = 0; i < r->route_count; i++) {
    if (i < r->snr_towards_count && r->snr_towards[i] != INT8_MIN)
      ESP_LOGI(TAG, "0x%x (%.2fdB) -->", r->route[i], (float) r->snr_towards[i] / 4);
    else
      ESP_LOGI(TAG, "0x%x (?dB) -->", r->route[i]);
  }
  if (request_id && r->snr_towards_count > 0 && r->snr_towards[r->snr_towards_count - 1] != INT8_MIN)
    ESP_LOGI(TAG, "0x%x (%.2fdB)", p->from, (float)r->snr_towards[r->snr_towards_count - 1] / 4);

  if(p->to == this->node_num_)
    ESP_LOGI(TAG, "0x%x (%.2fdB)", this->node_num_, p->rx_snr);
  if(r->route_back_count > 0) {
    for (int8_t i = r->route_back_count - 1; i >= 0; i--) {
      if (i < r->snr_back_count && r->snr_back[i] != INT8_MIN)
        ESP_LOGI(TAG, "(%.2fdB) 0x%x <--", (float) r->snr_back[i] / 4, r->route_back[i]);
      else
        ESP_LOGI(TAG, "(?dB) 0x%x <--", r->route_back[i]);
    }
  }
  if(p->to == this->node_num_) {
    if (r->snr_towards_count < ROUTE_SIZE) {
      r->snr_towards[r->snr_towards_count] = (int8_t)(p->rx_snr * 4); // Convert SNR to 1 byte
      r->snr_towards_count += 1;
    }
    meshtastic_MeshPacket mp;
    memset(&mp, 0, sizeof(mp));
    mp.to = p->from;
    mp.from = this->node_num_;
    mp.id = generatePacketId();
    mp.channel = p->channel;
    mp.priority = meshtastic_MeshPacket_Priority_ACK;
    mp.hop_limit = get_hop_limit4response(p->hop_start, p->hop_limit);
    mp.want_ack = false;
    mp.via_mqtt = false;

    mp.decoded.portnum = meshtastic_PortNum_TRACEROUTE_APP;
    mp.decoded.request_id = p->id;
    mp.decoded.payload.size = pb_encode_to_bytes(mp.decoded.payload.bytes, sizeof(mp.decoded.payload.bytes), &meshtastic_RouteDiscovery_msg, r);

    transmit_packet_encrypt(mp);
  }
#endif
}

void Meshtastic::handle_received_packet(meshtastic_MeshPacket &mp) {
  int hops_away = 0;
  std::string channel_name;

  if (mp.hop_start != 0 && mp.hop_limit <= mp.hop_start)
    hops_away = mp.hop_start - mp.hop_limit;
  ESP_LOGI(TAG, "id=%x fr=%x to=%x ch=0x%x next_hop=%x relay_node=%x hop=%d/%d away=%d rssi=%d snr=%.2f mqtt=%d", mp.id, mp.from, mp.to, mp.channel, mp.next_hop, mp.relay_node, mp.hop_start, mp.hop_limit, hops_away, mp.rx_rssi, mp.rx_snr, mp.via_mqtt);
  if(mp.from == this->node_num_) {
    ESP_LOGD(TAG, "Ignoring our own packet");
    return;
  }
  if (packet_history_.wasSeenRecently(mp, false)) {
    ESP_LOGD(TAG, "Ignoring recently seen packet");
    return;
  }
  if(this->ignore_mqtt_ && mp.via_mqtt) {
    ESP_LOGD(TAG, "Ignoring MQTT sourced packet");
    return;
  }

  uint8_t decrypted_data[MAX_LORA_PAYLOAD_LEN];
  size_t payload_len = mp.encrypted.size;
  if(mp.channel == 0 && mp.to == this->node_num_) {
#if !defined(MESHTASTIC_EXCLUDE_PKI)
    Node* src_node = get_node_by_num(mp.from);
    if(src_node == nullptr) {
      ESP_LOGW(TAG, "No node found for num 0x%08x to decrypt with Curve25519", mp.from);
      return;
    }
    if(!decrypt_curve25519(mp.from, src_node->public_key_.data(), mp.id, payload_len, mp.encrypted.bytes, decrypted_data)) {
      ESP_LOGW(TAG, "Failed to decrypt Curve25519 payload from 0x%08x", mp.from);
      return;
    }
    payload_len -= 12; // auth size
    ESP_LOGD(TAG, "decrypted Curve25519 payload %s", format_hex_pretty(decrypted_data, payload_len, ',', true).c_str());
#else
  ESP_LOGW(TAG, "PKI disabled. Failed to decrypt Curve25519 payload from 0x%08x", mp.from);
  return;
#endif
  } else {
    auto *channel = get_channel_by_hash(mp.channel);
    if(channel == nullptr) {
      ESP_LOGW(TAG, "No channel found for hash 0x%02x", mp.channel);
      return;
    }
    channel_name = channel->get_name();
    psk_t key = channel->get_psk();
    if(key.empty()) {
      memcpy(decrypted_data, mp.encrypted.bytes, payload_len);
      ESP_LOGD(TAG, "data unencrypted payload %s", format_hex_pretty(decrypted_data, payload_len, ',', true).c_str());
    }
    else
    {
      aes_decrypt_meshtastic_payload(key.data(), key.size() * 8, mp.id, mp.from, mp.encrypted.bytes, decrypted_data, payload_len);
      ESP_LOGD(TAG, "decrypted payload %s", format_hex_pretty(decrypted_data, payload_len, ',', true).c_str());
    }
  }
  meshtastic_Data decodedtmp;
  memset(&decodedtmp, 0, sizeof(decodedtmp));
  if(pb_decode_from_bytes(decrypted_data, payload_len, &meshtastic_Data_msg, &decodedtmp)) {
    ESP_LOGI(TAG, "Decoded meshtastic_Data request_id=%x ch_name=%s portnum=%d payload len=%d want_response=%d", decodedtmp.request_id, channel_name.c_str(), decodedtmp.portnum, decodedtmp.payload.size, decodedtmp.want_response);

    packet_history_.wasSeenRecently(mp);

    std::vector<uint8_t> decoded_packet(decodedtmp.payload.bytes, decodedtmp.payload.bytes + decodedtmp.payload.size);
    this->packet_listeners_.call(mp.from, mp.to, decodedtmp.portnum, decoded_packet);

    if (decodedtmp.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
      std::string msg = std::string(reinterpret_cast<const char*>(decodedtmp.payload.bytes), decodedtmp.payload.size);
      ESP_LOGI(TAG, "Received meshtastic message: %s\n", msg.c_str());
    }
    else if(decodedtmp.portnum == meshtastic_PortNum_POSITION_APP) {
      ESP_LOGI(TAG, "Received meshtastic position\n");
      meshtastic_Position position = {};
      float lat;
      float lng;
      if (pb_decode_from_bytes(decodedtmp.payload.bytes, decodedtmp.payload.size, &meshtastic_Position_msg, &position)) {
        lat = position.latitude_i * 1e-7; // Convert from Meshtastic's internal  int32_t format
        lng = position.longitude_i * 1e-7;
        ESP_LOGI(TAG, "Position: lat=%f lng=%f alt=%d\n", lat, lng, position.altitude);
        if(decodedtmp.want_response && mp.to == this->node_num_) {
          send_our_node_position(mp.from, mp.channel, meshtastic_MeshPacket_Priority_RELIABLE);
        }
        pb_release(&meshtastic_Position_msg, &position);
      }
    }
    else if(decodedtmp.portnum == meshtastic_PortNum_NODEINFO_APP) {
      ESP_LOGI(TAG, "Received meshtastic Node Info\n");
      meshtastic_User user_msg = {};
      if (pb_decode_from_bytes(decodedtmp.payload.bytes, decodedtmp.payload.size, &meshtastic_User_msg, &user_msg)) {
        ESP_LOGI(TAG, "Node Info: %s (%s) %s\n", user_msg.long_name, user_msg.short_name, format_hex_pretty(user_msg.macaddr, 6, ':', false).c_str());
        if(decodedtmp.want_response && mp.to == this->node_num_) {
          send_our_node_info(mp.from, mp.channel, meshtastic_MeshPacket_Priority_DEFAULT);
        }
        pb_release(&meshtastic_User_msg, &user_msg);
      }
    }
    else if(decodedtmp.portnum == meshtastic_PortNum_ROUTING_APP) {
      ESP_LOGI(TAG, "Received meshtastic routing\n");
      meshtastic_Routing routing_msg = {};
      if (pb_decode_from_bytes(decodedtmp.payload.bytes, decodedtmp.payload.size, &meshtastic_Routing_msg, &routing_msg)) {
        ESP_LOGI(TAG, "Routing: varinat %d\n", routing_msg.which_variant);
        if(routing_msg.which_variant == meshtastic_Routing_error_reason_tag) {
          ESP_LOGI(TAG, "Routing Error: reason=%d\n", routing_msg.error_reason);
        }
        pb_release(&meshtastic_Routing_msg, &routing_msg);
      }
    }
    else if(decodedtmp.portnum == meshtastic_PortNum_DETECTION_SENSOR_APP) {
      std::string msg = std::string(reinterpret_cast<const char*>(decodedtmp.payload.bytes), decodedtmp.payload.size);
      ESP_LOGI(TAG, "Received meshtastic detection sensor: %s\n", msg.c_str());
    }
    else if(decodedtmp.portnum == meshtastic_PortNum_ALERT_APP) {
      std::string msg = std::string(reinterpret_cast<const char*>(decodedtmp.payload.bytes), decodedtmp.payload.size);
      ESP_LOGI(TAG, "Received meshtastic alert: %s\n", msg.c_str());
    }
    else if(decodedtmp.portnum == meshtastic_PortNum_TELEMETRY_APP) {
      ESP_LOGI(TAG, "Received meshtastic telemetry\n");
      meshtastic_Telemetry telemetry_msg = {};
      if (pb_decode_from_bytes(decodedtmp.payload.bytes, decodedtmp.payload.size, &meshtastic_Telemetry_msg, &telemetry_msg)) {
        ESP_LOGI(TAG, "Telemetry: varinat %d\n", telemetry_msg.which_variant);
        if(telemetry_msg.which_variant == meshtastic_Telemetry_environment_metrics_tag) {
          ESP_LOGI(TAG, "Telemetry Env: temperature=%f humidity=%f pressure=%f\n",
            telemetry_msg.variant.environment_metrics.temperature,
            telemetry_msg.variant.environment_metrics.relative_humidity,
            telemetry_msg.variant.environment_metrics.barometric_pressure);
        } else if(telemetry_msg.which_variant == meshtastic_Telemetry_device_metrics_tag) {
          ESP_LOGI(TAG, "Telemetry Device: voltage=%f channel_utilization=%.2f air_util_tx=%.2f uptime_seconds=%d\n",
            telemetry_msg.variant.device_metrics.voltage,
            telemetry_msg.variant.device_metrics.channel_utilization,
            telemetry_msg.variant.device_metrics.air_util_tx,
            telemetry_msg.variant.device_metrics.uptime_seconds);
        }
        pb_release(&meshtastic_Telemetry_msg, &telemetry_msg);
      }
    }
    else if(decodedtmp.portnum == meshtastic_PortNum_TRACEROUTE_APP) {
      ESP_LOGI(TAG, "Received meshtastic traceroute\n");
      meshtastic_RouteDiscovery route_discovery_msg = {};
      if (pb_decode_from_bytes(decodedtmp.payload.bytes, decodedtmp.payload.size, &meshtastic_RouteDiscovery_msg, &route_discovery_msg)) {
        ESP_LOGI(TAG, "RouteDiscovery: route_count=%d, route_back_count=%d, snr_towards_count=%d, snr_back_count=%d", route_discovery_msg.route_count, route_discovery_msg.route_back_count, route_discovery_msg.snr_towards_count, route_discovery_msg.snr_back_count);
        handle_traceroute(&mp, decodedtmp.request_id, &route_discovery_msg);
        pb_release(&meshtastic_RouteDiscovery_msg, &route_discovery_msg);
      }
    }
    else if(decodedtmp.portnum == meshtastic_PortNum_PRIVATE_APP) {
      ESP_LOGI(TAG, "Received meshtastic private app data\n");
    }
    pb_release(&meshtastic_Data_msg, &decodedtmp);
  }
  else
  {
    ESP_LOGE(TAG, "Failed to decode meshtastic_Data_msg");
    rx_bad_count++;
  }
  if(mp.want_ack && mp.to == this->node_num_) {
    send_ack(mp.from, mp.id, mp.channel, meshtastic_Routing_Error_NONE);
  }
}

void Meshtastic::on_udp_packet(const std::vector<uint8_t> &packet, struct sockaddr_in *addr) {
  ESP_LOGD(TAG, "on_udp_packet %s %s", inet_ntoa(addr->sin_addr), format_hex_pretty(packet, ',', true).c_str());

  rx_count_udp++;
  ESP_LOGI("CNT", "rx count lora=%d udp=%d/%d bad=%d", rx_count_lora, rx_count_udp, rx_bad_udp, rx_bad_count);

  size_t packet_length = packet.size();

  meshtastic_MeshPacket mp;
  bool is_packet_decoded = pb_decode_from_bytes(packet.data(), packet_length, &meshtastic_MeshPacket_msg, &mp);
  if(!is_packet_decoded) {
    ESP_LOGE(TAG, "Failed to decode meshtastic_MeshPacket_msg");
    rx_bad_udp++;
    return;
  }
  mp.transport_mechanism = meshtastic_MeshPacket_TransportMechanism_TRANSPORT_MULTICAST_UDP;
  mp.pki_encrypted = false;
  mp.public_key.size = 0;
  memset(mp.public_key.bytes, 0, sizeof(mp.public_key.bytes));
//  mp.rx_rssi = 0;
//  mp.rx_snr = 0;
  handle_received_packet(mp);
  if(this->bridge_mode_) {
    // In bridge mode, retransmit UDP-received packets over LoRa
    if (!transmit_radio_packet(&mp)) {
      ESP_LOGW(TAG, "transmit_radio_packet timeout");
    }
  }
  pb_release(&meshtastic_MeshPacket_msg, &mp);
}

//LoRa packet received
void Meshtastic::on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) {
  ESP_LOGD(TAG, "on_packet %s", format_hex_pretty(packet, ',', true).c_str());

  rx_count_lora++;
  ESP_LOGI("CNT", "rx count lora=%d udp=%d/%d bad=%d", rx_count_lora, rx_count_udp, rx_bad_udp, rx_bad_count);

  RadioBuffer radio_buffer;
  std::copy(packet.begin(), packet.end(), (uint8_t *)&radio_buffer);
  int32_t payload_len = packet.size() - sizeof(PacketHeader);

  meshtastic_MeshPacket mp;
  memset(&mp, 0, sizeof(mp));
  mp.transport_mechanism = meshtastic_MeshPacket_TransportMechanism_TRANSPORT_LORA;
  mp.id = radio_buffer.header.id;
  mp.from = radio_buffer.header.from;
  mp.to = radio_buffer.header.to;
  mp.channel = radio_buffer.header.channel;
  mp.hop_limit = radio_buffer.header.flags & PACKET_FLAGS_HOP_LIMIT_MASK;
  mp.hop_start = (radio_buffer.header.flags & PACKET_FLAGS_HOP_START_MASK) >> PACKET_FLAGS_HOP_START_SHIFT;
  mp.want_ack = !!(radio_buffer.header.flags & PACKET_FLAGS_WANT_ACK_MASK);
  mp.via_mqtt = !!(radio_buffer.header.flags & PACKET_FLAGS_VIA_MQTT_MASK);
  mp.next_hop = radio_buffer.header.next_hop;
  mp.relay_node = radio_buffer.header.relay_node;
  mp.encrypted.size = payload_len;
  memcpy(mp.encrypted.bytes, radio_buffer.payload, payload_len);
  mp.rx_snr = snr;
  mp.rx_rssi = rssi;
  mp.rx_time = ::time(nullptr);
  handle_received_packet(mp);
  if(this->bridge_mode_ && this->use_udp_) {
    transmit_udp_packet(&mp);
  }
  pb_release(&meshtastic_MeshPacket_msg, &mp);
}

}  // namespace meshtastic
}  // namespace esphome
