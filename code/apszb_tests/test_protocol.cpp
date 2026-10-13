// Host-side unit tests for aps_protocol.cpp
//   g++ -std=c++17 -Wall -Wextra -I../components/apszb test_protocol.cpp ../components/apszb/aps_protocol.cpp -o test_protocol && ./test_protocol
#include "aps_protocol.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace esphome::apszb;

static int failures = 0;
#define CHECK(cond)                                                                 \
  do {                                                                              \
    if (!(cond)) {                                                                  \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                        \
      failures++;                                                                   \
    }                                                                               \
  } while (0)
#define CHECK_NEAR(a, b, tol)                                                       \
  do {                                                                              \
    if (std::fabs(double(a) - double(b)) > (tol)) {                                 \
      printf("FAIL %s:%d  %s=%f expected %f\n", __FILE__, __LINE__, #a, double(a), double(b)); \
      failures++;                                                                   \
    }                                                                               \
  } while (0)

static std::vector<uint8_t> hex(const std::string &s) {
  std::vector<uint8_t> v(s.size() / 2);
  parse_hex_bytes(s.c_str(), v.data(), v.size());
  return v;
}

int main() {
  NetworkConfig cfg;
  uint8_t ecu[6];
  parse_hex_bytes("D8A3011B9780", ecu, 6);
  cfg.set_ecu_id(ecu);
  CHECK(cfg.pan_id == 0xA3D8);
  uint8_t rev[6];
  cfg.ecu_reverse(rev);
  CHECK(memcmp(rev, hex("80971B01A3D8").data(), 6) == 0);
  uint8_t ieee[8];
  cfg.ieee_address(ieee);
  CHECK(memcmp(ieee, hex("FFFF80971B01A3D8").data(), 8) == 0);

  // ---- application payloads must match the strings used by the original firmware ----
  uint8_t a[128];
  size_t n = asdu_poll(a, rev);
  CHECK(n == 19);
  CHECK(memcmp(a, hex("80971B01A3D8FBFB06BB000000000000C1FEFE").data(), n) == 0);
  n = asdu_normal_ops(a, rev);
  CHECK(n == 30);
  n = asdu_throttle(a, rev, InverterType::DS3, 100);  // sniffed ECU-C frame: fbfb06aa270000067b0158fefe
  CHECK(memcmp(a + 6, hex("FBFB06AA270000067B0158FEFE").data(), n - 6) == 0);
  n = asdu_throttle(a, rev, InverterType::YC600, 300);
  CHECK(memcmp(a + 6, hex("FBFB061C8C0221DB0001ACFEFE").data(), n - 6) == 0);

  // ---- pairing payload lengths (17 / 6 / 17 / 6) ----
  uint8_t serial[6];
  parse_hex_bytes("408000158215", serial, 6);
  uint16_t cl;
  CHECK(asdu_pair(a, 0, cfg, serial, &cl) == 17 && cl == 0x020D);
  CHECK(memcmp(a + 6, hex("FFFF10FFFF").data(), 5) == 0);
  CHECK(asdu_pair(a, 1, cfg, serial, &cl) == 6 && cl == 0x020C);
  CHECK(asdu_pair(a, 2, cfg, serial, &cl) == 17 && cl == 0x010F);
  CHECK(memcmp(a + 6, hex("A3D810FFFF").data(), 5) == 0);
  CHECK(asdu_pair(a, 3, cfg, serial, &cl) == 6 && cl == 0x0101);

  // ---- DS3 query answer (original firmware test fixture): programmed max power 600 W ----
  {
    auto v = hex(
        "704000202594FBFB5CDDDE010426E20013BA14B413EC000A032000500003DD03A403200003E80000000000640003DD03A503350304012C060D"
        "03FF045F0E93140E3204890258001374136F125C0014032007D023A6031401BF03D9FFFFFFFFFFFF23A6C8FF1C01FEFE");
    float w;
    CHECK(decode_max_power(v.data(), v.size(), InverterType::DS3, &w));
    CHECK_NEAR(w, 600.0, 0.1);
    CHECK(asdu_has_serial(v.data(), v.size(), hex("704000202594").data()));
  }
  // ---- DS3 poll answer, sample from the comments of the original decoder (README: dcv 36.8/37.0, f 50.0) ----
  {
    auto v = hex(
        "703000021300FBFB5CBBBB2000FC0001FFFF000000000000000006E506EE015901DA036E13882BBB01480026FFFF05250843"
        "003A40B20035385200FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    PollData d;
    CHECK(decode_poll(v.data(), v.size(), InverterType::DS3, d));
    printf("DS3 poll sample: acv=%.1f f=%.2f T=%.1f dcv=%.1f/%.1f dcc=%.2f/%.2f ts=%u en=%.2f/%.2f Wh\n", d.ac_voltage,
           d.frequency, d.temperature, d.dc_voltage[0], d.dc_voltage[1], d.dc_current[0], d.dc_current[1], d.timestamp,
           d.energy_wh[0], d.energy_wh[1]);
    CHECK_NEAR(d.dc_voltage[0], 36.8, 0.1);
    CHECK_NEAR(d.dc_voltage[1], 37.0, 0.1);
    CHECK_NEAR(d.frequency, 50.0, 0.01);
    CHECK_NEAR(d.dc_current[0], 4.3, 0.05);
  }
  // ---- YC600 answer: real bytes from the comments of ZIGBEE decode (tail of an AF_INCOMING_MSG) ----
  {
    std::string s = "408000158215FBFB51B103D40F411700007 4CF000000767 06A73D06B04960000";
    std::string t;
    for (char c : s)
      if (c != ' ')
        t += c;
    t += std::string(120, '0');  // padding, energy counters not checked here
    auto v = hex(t);
    PollData d;
    CHECK(decode_poll(v.data(), v.size(), InverterType::YC600, d));
    printf("YC600 sample: acv=%.1f f=%.2f T=%.1f dcv=%.1f/%.1f dcc=%.2f/%.2f ts=%u\n", d.ac_voltage, d.frequency,
           d.temperature, d.dc_voltage[0], d.dc_voltage[1], d.dc_current[0], d.dc_current[1], d.timestamp);
    CHECK_NEAR(d.ac_voltage, 221.1, 0.2);
    CHECK_NEAR(d.frequency, 50.01, 0.02);
    CHECK_NEAR(d.dc_voltage[1], 34.3, 0.1);
    CHECK_NEAR(d.dc_voltage[0], 34.7, 0.1);
    CHECK(d.timestamp == 0x74CF);
  }
  // ---- YC600 query answer fixture: max power sits before 3B 66 ----
  {
    auto v = hex(
        "408000158215FBFB4DDE041105440FE5020F32B003CF05440FE5020F32B0066604CC0EA3A804D70214050C100FD80ED07A0F32B0056A054F"
        "019000641F3FE480068ACE8ACE000130103030190604001D21DB3B6600000000FEFE");
    float w;
    CHECK(decode_max_power(v.data(), v.size(), InverterType::YC600, &w));
    CHECK_NEAR(w, 300.0, 0.1);
  }

  // ---- frame round trip ----
  FrameFactory ff(&cfg);
  TxFrame f;
  uint8_t asdu[64];
  size_t an = asdu_poll(asdu, rev);
  CHECK(ff.build_unicast(f, 0x103A, CLUSTER_ECU_TO_INV, asdu, an));
  CHECK(f.buf[0] == f.mac_len + 2);
  RxFrame rx;
  CHECK(parse_frame(f.buf + 1, f.mac_len, rx));
  CHECK(rx.mac_ok && rx.nwk_ok && rx.aps_ok);
  CHECK(rx.mac_pan == 0xA3D8 && rx.mac_dst == 0x103A && rx.mac_src == 0);
  CHECK(rx.nwk_dst == 0x103A && rx.nwk_src == 0 && rx.nwk_version == 2);
  CHECK(rx.cluster == 0x0006 && rx.profile == 0x0F05 && rx.dst_ep == 0x14 && rx.src_ep == 0x14);
  CHECK(rx.payload_len == an && memcmp(rx.payload, asdu, an) == 0);
  printf("unicast poll frame: %zu bytes MAC: ", f.mac_len);
  for (size_t i = 0; i < f.mac_len; i++) printf("%02X", f.buf[1 + i]);
  printf("\n");

  CHECK(ff.build_broadcast(f, 0xFFFF, CLUSTER_PAIR_1, serial, 6));
  CHECK(parse_frame(f.buf + 1, f.mac_len, rx));
  CHECK(rx.aps_ok && rx.aps_delivery == 2 && rx.mac_dst == 0xFFFF && rx.mac_pan == 0xFFFF && rx.cluster == 0x020C);

  CHECK(ff.build_many_to_one_rreq(f));
  CHECK(parse_frame(f.buf + 1, f.mac_len, rx));
  CHECK(rx.nwk_ok && rx.nwk_type == 1 && rx.nwk_cmd == 0x01 && rx.nwk_dst == 0xFFFC);
  uint16_t nb[2] = {0x103A, 0x6C22};
  CHECK(ff.build_link_status(f, nb, 2));
  CHECK(parse_frame(f.buf + 1, f.mac_len, rx));
  CHECK(rx.nwk_cmd == 0x08);

  // ---- incoming frame with source routing flag + APS ack request + fragmentation ----
  {
    // MAC (9) + NWK with src-route (FC 0x0408) ...
    std::vector<uint8_t> fr = hex("61887A" "D8A3" "0000" "3A10");
    auto nwk = hex("0804" "0000" "3A10" "0E" "11" "0000");  // FC, dst, src, radius, seq, relay count 0, idx 0
    fr.insert(fr.end(), nwk.begin(), nwk.end());
    auto aps = hex("C0" "14" "0601" "050F" "14" "42" "01" "02");  // ack req + ext header; frag=first, block count=2
    // wait: ext header: ext FC (frag=1) then block number (=2 blocks). Build properly:
    aps = hex("C0" "14" "0601" "050F" "14" "42" "01" "02");
    fr.insert(fr.end(), aps.begin(), aps.end());
    auto pl = hex("AABBCC");
    fr.insert(fr.end(), pl.begin(), pl.end());
    RxFrame r1;
    CHECK(parse_frame(fr.data(), fr.size(), r1));
    CHECK(r1.aps_ok && r1.aps_ack_request && r1.aps_ext && r1.frag == 1 && r1.block == 2 && r1.payload_len == 3);
    CHECK(r1.cluster == 0x0106 && r1.nwk_src == 0x103A);
    Reassembler ra;
    const uint8_t *o;
    size_t on;
    CHECK(!ra.feed(r1, 1000, &o, &on));
    RxFrame r2 = r1;
    uint8_t second[2] = {0xDD, 0xEE};
    r2.frag = 2;
    r2.block = 1;
    r2.payload = second;
    r2.payload_len = 2;
    CHECK(ra.feed(r2, 1100, &o, &on));
    CHECK(on == 5 && o[0] == 0xAA && o[3] == 0xDD && o[4] == 0xEE);
    CHECK(ff.build_aps_ack(f, r1));
    RxFrame ack;
    CHECK(parse_frame(f.buf + 1, f.mac_len, ack));
    CHECK(ack.aps_ok && ack.aps_type == 2 && ack.cluster == 0x0106 && ack.aps_counter == 0x42);
  }

  {
    // log formatting helpers (independent of the ESPHome core helpers)
    const uint8_t d[3] = {0x00, 0xAB, 0x7F};
    CHECK(hex_dump(d, 3) == "00 AB 7F");
    CHECK(hex_dump(d, 0).empty());
    CHECK(addr_hex(0x103A) == "0x103A" && addr_hex(0x0000) == "0x0000" && addr_hex(0xFFFF) == "0xFFFF");
  }

  printf(failures ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", failures);
  return failures ? 1 : 0;
}
