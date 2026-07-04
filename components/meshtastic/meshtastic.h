#pragma once

#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/components/network/ip_address.h"
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
#include "esphome/components/socket/socket.h"
#endif
#endif
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#if __has_include("esphome/components/sx126x/sx126x.h")
#include "esphome/components/sx126x/sx126x.h"
#define USE_SX126X 1
#endif

#if __has_include("esphome/components/sx127x/sx127x.h")
#include "esphome/components/sx127x/sx127x.h"
#define USE_SX127X 1
#endif

#include "mesh.pb.h"

#include <unordered_set>


#define NODENUM_BROADCAST UINT32_MAX
//#define UDP_MULTICAST_IP "224.0.0.69"
#define UDP_MULTICAST_DEFAUL_PORT 4403 // Default port for UDP multicast is same as TCP api server


namespace esphome {
namespace meshtastic {

typedef uint32_t NodeNum;
typedef uint32_t PacketId;

typedef struct {
    NodeNum to, from;
    PacketId id;
    uint8_t flags;
    uint8_t channel;
    uint8_t next_hop;
    uint8_t relay_node;
} PacketHeader;

#define MAX_LORA_PAYLOAD_LEN 255

typedef struct {
    PacketHeader header;
    uint8_t payload[MAX_LORA_PAYLOAD_LEN + 1 - sizeof(PacketHeader)] __attribute__((__aligned__));
} RadioBuffer;

using psk_t = std::vector<uint8_t>;

//    uint8_t default_secret[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59, 0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01};

struct Node {
    std::string name_;
    std::string name_short_;
    NodeNum node_num_;
    std::vector<uint8_t> private_key_;
    std::vector<uint8_t> public_key_;
};

class Channel {
 private:
  std::string name_;
  psk_t psk_;
  uint8_t hash_;
 public:
  void set_name(const std::string &name) { this->name_ = name; };
  std::string get_name() const { return this->name_; };
  void set_psk(const psk_t &psk) { this->psk_ = psk; };
  psk_t get_psk() const { return this->psk_; };
  void generate_hash();
  void set_hash(uint8_t hash) { this->hash_ = hash; };
  uint8_t get_hash() const { return this->hash_; };
};

#define MAX_NUM_NODES 100
#define FLOOD_EXPIRE_TIME (10 * 60 * 1000L)

struct PacketRecord {
    NodeNum sender;
    PacketId id;
    uint32_t rxTimeMsec;
    bool operator==(const PacketRecord &p) const { return sender == p.sender && id == p.id; }
};

class PacketRecordHashFunction
{
  public:
    size_t operator()(const PacketRecord &p) const { return (std::hash<NodeNum>()(p.sender)) ^ (std::hash<PacketId>()(p.id)); }
};

class PacketHistory
{
  private:
    std::unordered_set<PacketRecord, PacketRecordHashFunction> recentPackets;
    void clearExpiredRecentPackets();
  public:
    PacketHistory(){ recentPackets.reserve(MAX_NUM_NODES); }
    bool wasSeenRecently(const meshtastic_MeshPacket &p, bool withUpdate = true);
};


class Meshtastic : public Component
#ifdef USE_SX126X
  , sx126x::SX126xListener
#endif
#ifdef USE_SX127X
  , sx127x::SX127xListener
#endif
{
 private:
#ifdef USE_SX126X
  sx126x::SX126x* sx126x_ = nullptr;
#endif
#ifdef USE_SX127X
  sx127x::SX127x* sx127x_ = nullptr;
#endif
  NodeNum node_num_;
  meshtastic_User owner;
  Node *owner_node_ = nullptr;
  meshtastic_Position owner_position = meshtastic_Position_init_default;

  uint32_t node_broadcast_interval_ = (3*60*60*1000);
  uint32_t position_broadcast_interval_ = (3*60*60*1000);

  std::vector<Channel> channels_;
  std::vector<std::unique_ptr<Node>> nodes_;

#if !defined(MESHTASTIC_EXCLUDE_PKI)
  bool init_shared_key(uint8_t* shared_key, uint8_t* pubKey);
  bool encrypt_curve25519(uint32_t toNode, uint32_t fromNode, uint8_t* remotePublic, uint64_t packetNum, size_t numBytes, const uint8_t* bytes, uint8_t* bytesOut);
  bool decrypt_curve25519(uint32_t fromNode, uint8_t* remotePublic, uint64_t packetNum, size_t numBytes, const uint8_t* bytes, uint8_t* bytesOut);
#endif
  int rx_count_lora = 0;
  int rx_count_udp = 0;
  int rx_bad_udp = 0;
  int rx_bad_count = 0;
 protected:
  CallbackManager<void(uint32_t from, uint32_t to, int32_t port, std::vector<uint8_t> &data)> packet_listeners_{};

  bool transmit_radio_packet(meshtastic_MeshPacket *mp);
  bool transmit_udp_packet(meshtastic_MeshPacket *mp, int ntries = 0);
  void on_udp_packet(const std::vector<uint8_t> &packet, sockaddr_in *addr);
  void handle_received_packet(meshtastic_MeshPacket &mp);
  Channel* get_channel_by_hash(uint8_t hash);
  Channel* get_channel_by_name(const std::string &name);
  PacketId generatePacketId();
  Node* get_node_by_num(const NodeNum num);

  uint8_t hop_limit_ = 7;
  bool ignore_mqtt_ = true;
  bool ok_to_mqtt_ = false;
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  std::unique_ptr<socket::Socket> broadcast_socket_ = nullptr;
  std::unique_ptr<socket::Socket> listen_socket_ = nullptr;
  std::vector<struct sockaddr> sockaddrs_{};
  std::vector<std::string> addresses_{};
  uint16_t listen_port_ = UDP_MULTICAST_DEFAUL_PORT;
  uint16_t broadcast_port_ = UDP_MULTICAST_DEFAUL_PORT;

  bool use_udp_ = false;
  bool bridge_mode_ = false;
#endif
  optional<network::IPAddress> multicast_address_{};
  PacketHistory packet_history_;

  uint8_t get_hop_limit4response(uint8_t hop_start, uint8_t hop_limit);
  void send_ack(NodeNum to, PacketId id_from, uint8_t channel, meshtastic_Routing_Error err);
  void handle_traceroute(meshtastic_MeshPacket *p, PacketId req_id, meshtastic_RouteDiscovery *route_discovery_msg);

 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; };

#ifdef USE_SX126X
  void set_lora(sx126x::SX126x* sx);
#endif
#ifdef USE_SX127X
  void set_lora(sx127x::SX127x* sx);
#endif
  void add_channel(const std::string &name, const std::vector<uint8_t> &psk) {this->channels_.push_back(Channel()); this->channels_.back().set_name(name); this->channels_.back().set_psk(psk); this->channels_.back().generate_hash(); };
  void add_node(NodeNum node_num, const std::string &name, const std::string &short_name, const std::vector<uint8_t> &public_key, const std::vector<uint8_t> &private_key) {
      auto node = std::make_unique<Node>();
      node->name_ = name;
      node->name_short_ = short_name;
      node->node_num_ = node_num;
      node->private_key_ = private_key;
      node->public_key_ = public_key;
      if(node_num == 0) {
          std::strcpy(owner.long_name, name.c_str());
          std::strcpy(owner.short_name, short_name.c_str());
          if(public_key.size() == 32 && private_key.size() == 32) {
            std::memcpy(owner.public_key.bytes, public_key.data(), public_key.size());
            owner.public_key.size = public_key.size();
          }
          this->owner_node_ = node.get();
      }
      this->nodes_.push_back(std::move(node));
  };
  void set_hw_model(int hw_model) {
      owner.hw_model = static_cast<meshtastic_HardwareModel>(hw_model);
  };
  void set_hop_limit(uint8_t hop_limit) { this->hop_limit_ = hop_limit; };
  void set_ignore_mqtt(bool ignore_mqtt) { this->ignore_mqtt_ = ignore_mqtt; };
  void set_ok_to_mqtt(bool ok_to_mqtt) { this->ok_to_mqtt_ = ok_to_mqtt; };
  void set_node_broadcast_interval(uint32_t broadcast_interval) { this->node_broadcast_interval_ = broadcast_interval; };
  void set_position_broadcast_interval(uint32_t broadcast_interval) { this->position_broadcast_interval_ = broadcast_interval; };
  void set_position(float latitude, float longitude, float altitude) {
      this->owner_position.has_latitude_i = true;
      this->owner_position.latitude_i = static_cast<int32_t>(latitude * 1e7);
      this->owner_position.has_longitude_i = true;
      this->owner_position.longitude_i = static_cast<int32_t>(longitude * 1e7);
      this->owner_position.has_altitude = true;
      this->owner_position.altitude = altitude;
      this->owner_position.location_source = meshtastic_Position_LocSource_LOC_MANUAL;
      this->owner_position.altitude_source = meshtastic_Position_AltSource_ALT_MANUAL;
  };

  void set_use_udp(bool use_udp) { this->use_udp_ = use_udp; };
  void set_multicast_address(const char *multicast_addr) { this->multicast_address_ = network::IPAddress(multicast_addr); }
  void add_address(const char *addr) { this->addresses_.emplace_back(addr); }
  void set_bridge_mode(bool bridge_mode) { this->bridge_mode_ = bridge_mode; }
  void add_listener(std::function<void(uint32_t from, uint32_t to, int32_t port, std::vector<uint8_t> &data)> &&listener) { this->packet_listeners_.add(std::move(listener)); }
  //packet transport
  size_t get_max_packet_size() {return MAX_LORA_PAYLOAD_LEN + 1 - sizeof(PacketHeader); };
  bool transmit_packet(const std::vector<uint8_t> &packet, uint8_t hop_limit);

  bool transmit_packet_encrypt(meshtastic_MeshPacket &mp, bool want_response = false);
  bool send_text_message(NodeNum to, const std::string &channel_name, const std::string &msg);
  void send_our_node_info(NodeNum to, uint8_t channel, meshtastic_MeshPacket_Priority priority);
  void send_our_node_position(NodeNum to, uint8_t channel, meshtastic_MeshPacket_Priority priority);

  //lora callbacks
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr);

  //protobuf helpers
  static bool pb_decode_from_bytes(const uint8_t* srcbuf, size_t srcbufsize, const pb_msgdesc_t* fields, void* dest_struct);
  static size_t pb_encode_to_bytes(uint8_t* destbuf, size_t destbufsize, const pb_msgdesc_t* fields, const void* src_struct);
};


}  // namespace meshtastic
}  // namespace esphome
