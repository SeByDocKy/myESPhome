#pragma once

// Pure protocol layer of the "apszb" component (no ESPHome / ESP-IDF dependency, so it can be unit-tested on a PC).
//
//  * builds / parses IEEE 802.15.4 MAC + ZigBee NWK + ZigBee APS frames (what the TI Z-Stack ZNP firmware of the CC2530 does
//    in the original ESP32-ECU project),
//  * builds the APsystems application payloads (poll, query, throttle, pairing, ...),
//  * decodes the answers of YC600 / QS1 / DS3 micro-inverters.
//
// All protocol constants were derived from https://github.com/patience4711/ESP32-read-APS-inverters.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace esphome {
namespace apszb {

// ---------------------------------------------------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------------------------------------------------
static const uint16_t APS_PROFILE_ID = 0x0F05;  // profile of the endpoint registered by the ECU (AF_REGISTER)
static const uint8_t APS_ENDPOINT = 0x14;       // endpoint used by ECU and inverters

static const uint16_t CLUSTER_ECU_TO_INV = 0x0006;  // poll / query / throttle / reboot / "normal operation"
static const uint16_t CLUSTER_INV_TO_ECU = 0x0106;  // answers of the inverters
static const uint16_t CLUSTER_PAIR_0 = 0x020D;
static const uint16_t CLUSTER_PAIR_1 = 0x020C;
static const uint16_t CLUSTER_PAIR_2 = 0x010F;
static const uint16_t CLUSTER_PAIR_3 = 0x0101;

static const uint16_t ADDR_BROADCAST = 0xFFFF;
static const uint16_t ADDR_ALL_ROUTERS = 0xFFFC;
static const uint16_t ADDR_COORDINATOR = 0x0000;

static const size_t MAX_PSDU = 127;     // IEEE 802.15.4 maximum PSDU (incl. 2 bytes FCS)
static const size_t MAX_ASDU = 100;     // generous upper bound for an APS payload in one frame
static const size_t REASSEMBLY_MAX = 256;
static const int8_t RSSI_UNKNOWN = -128;  // the CC2530 modem only reports an LQI

enum class InverterType : uint8_t { YC600 = 0, QS1 = 1, DS3 = 2 };

// ---------------------------------------------------------------------------------------------------------------------
// Network parameters (derived from the ECU id exactly like the original firmware does)
// ---------------------------------------------------------------------------------------------------------------------
struct NetworkConfig {
  uint8_t ecu_id[6] = {0};  // as printed on the ECU, e.g. D8 A3 01 1B 97 80
  uint16_t pan_id = 0;      // original firmware: PAN = (ecu_id[1] << 8) | ecu_id[0]
  uint8_t channel = 16;
  uint8_t nwk_protocol_version = 2;  // 2 = ZigBee PRO
  uint8_t radius = 15;
  bool discover_route = false;
  bool mac_ack = true;  // request an IEEE 802.15.4 acknowledgement for unicast frames

  void set_ecu_id(const uint8_t id[6]) {
    memcpy(this->ecu_id, id, 6);
    this->pan_id = (uint16_t(id[1]) << 8) | id[0];
  }
  // ecu id with the byte order reversed (this is what travels in the APS payloads)
  void ecu_reverse(uint8_t out[6]) const {
    for (int i = 0; i < 6; i++)
      out[i] = this->ecu_id[5 - i];
  }
  // IEEE (extended) address in little endian order as programmed in the CC2530: FF FF + reversed ecu id
  void ieee_address(uint8_t out[8]) const {
    out[0] = 0xFF;
    out[1] = 0xFF;
    this->ecu_reverse(out + 2);
  }
};

// ---------------------------------------------------------------------------------------------------------------------
// Frames
// ---------------------------------------------------------------------------------------------------------------------
// A frame ready for esp_ieee802154_transmit(): buf[0] = PSDU length (MAC frame + 2 bytes FCS), buf[1..] = MAC frame.
struct TxFrame {
  uint8_t buf[MAX_PSDU + 1];
  size_t mac_len = 0;  // MAC header + payload, without FCS
  bool ack_request = false;
  // Radio independent description of the same message (used by the ZNP / CC2530 backend, which builds its own frames)
  struct Msg {
    bool valid = false;      // false for NWK commands (link status, route request): nothing to send through a modem
    bool broadcast = false;
    uint16_t dst = 0;        // short address (unicast) or 0xFFFF
    uint16_t pan = 0;        // destination PAN of a broadcast (0xFFFF = pairing)
    uint16_t cluster = 0;
    uint8_t len = 0;
    uint8_t asdu[MAX_ASDU];
  } msg;
  void finish() {
    this->buf[0] = uint8_t(this->mac_len + 2);
  }
};

struct RxFrame {
  bool mac_ok = false;
  bool nwk_ok = false;
  bool aps_ok = false;
  uint8_t mac_type = 0;
  uint8_t mac_seq = 0;
  bool mac_ack_request = false;
  uint16_t mac_pan = 0;
  uint16_t mac_dst = 0;
  uint16_t mac_src = 0;
  // NWK
  uint8_t nwk_type = 0;  // 0 = data, 1 = command
  uint8_t nwk_version = 0;
  bool nwk_secured = false;
  uint16_t nwk_dst = 0;
  uint16_t nwk_src = 0;
  uint8_t nwk_radius = 0;
  uint8_t nwk_cmd = 0;  // first payload byte of a NWK command
  // APS
  uint8_t aps_type = 0;      // 0 = data, 1 = command, 2 = ack
  uint8_t aps_delivery = 0;  // 0 unicast, 2 broadcast, 3 group
  bool aps_ack_request = false;
  bool aps_secured = false;
  uint8_t dst_ep = 0;
  uint8_t src_ep = 0;
  uint16_t cluster = 0;
  uint16_t profile = 0;
  uint8_t aps_counter = 0;
  bool aps_ext = false;
  uint8_t frag = 0;  // 0 none, 1 first, 2 continuation
  uint8_t block = 0;
  const uint8_t *payload = nullptr;  // APS payload (ASDU)
  size_t payload_len = 0;
};

class FrameFactory {
 public:
  explicit FrameFactory(const NetworkConfig *cfg) : cfg_(cfg) {}

  // Unicast APS data frame to a short address (MAC ack requested)
  bool build_unicast(TxFrame &f, uint16_t dst, uint16_t cluster, const uint8_t *asdu, size_t n, bool aps_ack = false);
  // Broadcast APS data frame (NWK + MAC destination 0xFFFF). mac_pan = destination PAN in the MAC header.
  bool build_broadcast(TxFrame &f, uint16_t mac_pan, uint16_t cluster, const uint8_t *asdu, size_t n);
  // ZigBee NWK "link status" command (broadcast to routers). Neighbours get cost 1/1.
  bool build_link_status(TxFrame &f, const uint16_t *neighbours, size_t n);
  // ZigBee NWK many-to-one route request (concentrator announcement)
  bool build_many_to_one_rreq(TxFrame &f);
  // APS acknowledgement for a received frame that requested one
  bool build_aps_ack(TxFrame &f, const RxFrame &rx);

  uint8_t next_aps_counter() { return this->aps_counter_++; }

 protected:
  size_t put_mac_header_(uint8_t *p, bool ack_request, uint16_t dst_pan, uint16_t dst, uint16_t src);
  size_t put_nwk_header_(uint8_t *p, uint8_t frame_type, bool discover, uint16_t dst, uint16_t src, uint8_t radius);

  const NetworkConfig *cfg_;
  uint8_t mac_seq_{0};
  uint8_t nwk_seq_{0};
  uint8_t aps_counter_{0};
  uint8_t rreq_id_{0};
};

// Parse a received frame. `psdu` points to the MAC header, `len` is the number of bytes WITHOUT FCS.
bool parse_frame(const uint8_t *psdu, size_t len, RxFrame &out);

// ---------------------------------------------------------------------------------------------------------------------
// Fragment reassembly (APS fragmentation is used by the big answers)
// ---------------------------------------------------------------------------------------------------------------------
class Reassembler {
 public:
  // returns true when `out`/`out_len` hold a complete message
  bool feed(const RxFrame &rx, uint32_t now_ms, const uint8_t **out, size_t *out_len);
  void reset() { this->mask_ = 0; this->total_ = 0; }

 protected:
  uint8_t buf_[REASSEMBLY_MAX];
  size_t first_len_{0};
  size_t last_len_{0};
  uint8_t mask_{0};
  uint8_t total_{0};
  uint16_t src_{0};
  uint16_t cluster_{0};
  uint32_t started_{0};
};

// ---------------------------------------------------------------------------------------------------------------------
// APsystems application payloads (ASDU). All builders return the length written to `out`.
// ---------------------------------------------------------------------------------------------------------------------
size_t asdu_poll(uint8_t *out, const uint8_t ecu_rev[6]);
size_t asdu_query(uint8_t *out, const uint8_t ecu_rev[6]);
size_t asdu_query_flush(uint8_t *out, const uint8_t ecu_rev[6]);  // sent by the original firmware before the verification query
size_t asdu_reboot(uint8_t *out, const uint8_t ecu_rev[6]);
size_t asdu_normal_ops(uint8_t *out, const uint8_t ecu_rev[6]);
// `watts` already includes the calibration offset
size_t asdu_throttle(uint8_t *out, const uint8_t ecu_rev[6], InverterType type, int watts);

// pairing: step 0..3, `serial` is the 6 byte binary serial number of the inverter
size_t asdu_pair(uint8_t *out, int step, const NetworkConfig &cfg, const uint8_t serial[6], uint16_t *cluster);

// ---------------------------------------------------------------------------------------------------------------------
// Decoders
// ---------------------------------------------------------------------------------------------------------------------
struct PollData {
  float ac_voltage = 0;
  float frequency = 0;
  float temperature = 0;
  float dc_voltage[4] = {0};
  float dc_current[4] = {0};
  uint32_t timestamp = 0;      // inverter uptime counter (seconds)
  float energy_wh[4] = {0};    // energy counter per channel, converted to Wh
  uint8_t channels = 2;
};

// `asdu` is the APS payload received from the inverter (starts with the 6 bytes serial number).
bool decode_poll(const uint8_t *asdu, size_t len, InverterType type, PollData &out);
// Extract the programmed max power (W, still including the calibration offset) from an answer to a query command.
bool decode_max_power(const uint8_t *asdu, size_t len, InverterType type, float *watts);
// true if the ASDU starts with the given 6 byte serial
bool asdu_has_serial(const uint8_t *asdu, size_t len, const uint8_t serial[6]);
// serial helpers
bool parse_hex_bytes(const char *hex, uint8_t *out, size_t n);

// Formatting helpers for logs. They deliberately do not rely on the ESPHome core helpers (str_sprintf, format_hex_pretty
// returning a std::string), which were removed from recent ESPHome versions.
std::string hex_dump(const uint8_t *data, size_t len);  // "AA BB CC"
std::string addr_hex(uint16_t addr);                    // "0x103A"

}  // namespace apszb
}  // namespace esphome
