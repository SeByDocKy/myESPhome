// Host simulation of the apszb hub with a fake YC600 (0x103A) and a fake DS3 (pairs as 0x6C22, fragmented answers)
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <atomic>
#include <cmath>
#include <mutex>
#include <sstream>
#include <array>
#include <set>
#include <unordered_map>
#include <thread>
#include <csignal>
#include <cstdarg>
#include <climits>
#include <algorithm>
#define protected public
#define private public
#include "main.cpp"
#undef protected
#undef private

using namespace esphome::apszb;
using Clock = std::chrono::steady_clock;

static int failures = 0;
#define EXPECT(cond, ...) do { if (!(cond)) { printf("  FAIL: " __VA_ARGS__); printf("  [%s]\n", #cond); failures++; } else { printf("  ok:   " __VA_ARGS__); printf("\n"); } } while (0)

static std::vector<uint8_t> hex(const std::string &s) {
  std::vector<uint8_t> v(s.size() / 2);
  parse_hex_bytes(s.c_str(), v.data(), v.size());
  return v;
}

static uint8_t inv_seq = 0, inv_aps = 0;
// build an inverter -> ECU frame and queue it in the stub radio
static void push_inverter_frame(uint16_t src, const std::vector<uint8_t> &asdu, uint16_t cluster, bool ext = false,
                                uint8_t frag = 0, uint8_t block = 0, bool ack_req = false) {
  RadioRx r{};
  std::vector<uint8_t> f;
  auto p16 = [&](uint16_t v) { f.push_back(v & 0xFF); f.push_back(v >> 8); };
  p16(0x8861); f.push_back(inv_seq++); p16(0xA3D8); p16(0x0000); p16(src);       // MAC
  p16(0x0008); p16(0x0000); p16(src); f.push_back(14); f.push_back(inv_seq);      // NWK
  f.push_back(uint8_t((ext ? 0x80 : 0) | (ack_req ? 0x40 : 0)));                  // APS
  f.push_back(0x14); p16(cluster); p16(0x0F05); f.push_back(0x14); f.push_back(inv_aps);
  if (ext) { f.push_back(frag); f.push_back(block); }
  f.insert(f.end(), asdu.begin(), asdu.end());
  r.len = uint8_t(f.size()); r.rssi = -55; r.lqi = 200;
  memcpy(r.data, f.data(), f.size());
  aps->radio_.rx.push_back(r);
}

static std::vector<uint8_t> yc_answer(int k) {
  std::string s = "408000158215FBFB51B103D40F411700007 4CF000000767 06A73D06B0496";
  std::string t; for (char c : s) if (c != ' ') t += c;
  auto v = hex(t); v.resize(60, 0);
  uint16_t ts = 1000 + 5 * k;           v[17] = ts >> 8; v[18] = ts & 0xFF;
  uint32_t e0 = 1000 + 60 * k, e1 = 500 + 30 * k;  // +60 raw per 5 s on ch0 (~99.7 W), +30 on ch1
  v[37] = e0 >> 16; v[38] = e0 >> 8; v[39] = e0; v[42] = e1 >> 16; v[43] = e1 >> 8; v[44] = e1;
  return v;
}

static std::vector<uint8_t> ds3_answer(int k) {
  auto v = hex("703000021300FBFB5CBBBB2000FC0001FFFF000000000000000006E506EE015901DA036E13882BBB01480026FFFF052508430"
               "03A40B20035385200");
  std::vector<uint8_t> w(v.begin(), v.end());
  w.resize(66, 0);
  // the serial of the simulated DS3 is the one configured in the YAML
  uint16_t ts = 2000 + 5 * k; w[38] = ts >> 8; w[39] = ts & 0xFF;
  uint32_t e0 = 100000 + 6000 * k;  // raw units: Wh = raw/1000/100*1.66
  w[50] = e0 >> 24; w[51] = e0 >> 16; w[52] = e0 >> 8; w[53] = e0;
  return w;
}

static std::vector<uint8_t> yc_query_answer() {
  return hex("408000158215FBFB4DDE041105440FE5020F32B003CF05440FE5020F32B0066604CC0EA3A804D70214050C100FD80ED07A0F32B0056A054F"
             "019000641F3FE480068ACE8ACE000130103030190604001D21DB3B6600000000FEFE");
}

namespace esphome { namespace host { void setup_preferences(); } }
int main() {
  esphome::host::setup_preferences();
  setup();
  auto t0 = Clock::now();
  auto now_s = [&]() { return std::chrono::duration<double>(Clock::now() - t0).count(); };
  size_t seen = 0;
  int yc_polls = 0, ds_polls = 0, acks = 0, pair_cmds = 0, queries = 0;
  bool night = false, did_query = false, did_throttle = false;
  uint16_t last_ds3_dst = 0;
  bool paired_seen = false;
  float p1_before = NAN, p2_before = NAN;

  while (now_s() < 72.0) {
    loop();
    // react to frames sent by the hub
    while (seen < aps->radio_.sent.size()) {
      const TxFrame &tf = aps->radio_.sent[seen++];
      RxFrame rx;
      if (!parse_frame(tf.buf + 1, tf.mac_len, rx) || !rx.aps_ok) continue;
      if (rx.aps_type == 2) { acks++; continue; }
      if (rx.aps_type != 0 || !rx.payload) continue;
      if (night) continue;
      const uint8_t *a = rx.payload;
      size_t n = rx.payload_len;
      bool is_poll = n >= 12 && a[6] == 0xFB && a[8] == 0x06 && a[9] == 0xBB;
      bool is_query = n >= 12 && a[6] == 0xFB && a[8] == 0x06 && a[9] == 0xDE;
      if (rx.cluster == CLUSTER_ECU_TO_INV && rx.nwk_dst == 0x103A) {
        if (is_poll) { push_inverter_frame(0x103A, yc_answer(yc_polls++), 0x0106); }
        if (is_query) { queries++; push_inverter_frame(0x103A, yc_query_answer(), 0x0106); }
      } else if (rx.cluster == CLUSTER_ECU_TO_INV && rx.nwk_dst == 0x6C22) {
        last_ds3_dst = rx.nwk_dst;
        if (is_poll) {
          auto full = ds3_answer(ds_polls++);
          std::vector<uint8_t> p1(full.begin(), full.begin() + 40), p2(full.begin() + 40, full.end());
          push_inverter_frame(0x6C22, p1, 0x0106, true, 1, 2, true);
          push_inverter_frame(0x6C22, p2, 0x0106, true, 2, 1, true);
        }
      } else if (rx.cluster == CLUSTER_PAIR_1) {
        pair_cmds++;
        std::vector<uint8_t> asdu(a, a + 6);
        asdu.push_back(0x22); asdu.push_back(0x6C);  // short address LE
        if (memcmp(a, hex("703000021300").data(), 6) == 0) push_inverter_frame(0x6C22, asdu, 0x0106);
      }
    }
    // scripted user actions / events
    if (!did_query && now_s() > 22) { did_query = true; printf("[t=%.0fs] press 'query max power' (via number)\n", now_s()); inv1->request_query(); }
    if (!did_throttle && now_s() > 26) { did_throttle = true; printf("[t=%.0fs] set max power 250 W\n", now_s()); inv1->request_max_power(250); }
    if (!night && now_s() > 35.5 && std::isnan(p1_before)) { p1_before = inv1->power_[0]->state; p2_before = inv1->power_[1]->state; }
    if (!night && now_s() > 36) { night = true; printf("[t=%.0fs] NIGHT: inverters stop answering\n", now_s()); }
    if (!paired_seen && inv2->paired()) { paired_seen = true; printf("[t=%.1fs] inv2 paired as 0x%04X\n", now_s(), inv2->short_address()); }
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
  }

  printf("\n=== results ===\n");
  printf("yc polls answered: %d, ds3 polls answered: %d, aps acks sent: %d, queries: %d\n", yc_polls, ds_polls, acks, queries);
  EXPECT(inv2->short_address() == 0x6C22, "DS3 auto-paired as 0x6C22 (got 0x%04X)", inv2->short_address());
  EXPECT(yc_polls >= 4, "several YC600 polls were answered (%d)", yc_polls);
  EXPECT(ds_polls >= 2, "several DS3 polls were answered (%d)", ds_polls);
  EXPECT(acks >= 2 * ds_polls, "APS acks sent for every fragment (%d acks for %d polls)", acks, ds_polls);
  EXPECT(pair_cmds >= 1, "pairing broadcast seen (%d)", pair_cmds);
  EXPECT(inv1->ac_voltage_ && inv1->ac_voltage_->has_state() && std::fabs(inv1->ac_voltage_->state - 221.1f) < 0.3f, "YC600 AC voltage %.1f V", inv1->ac_voltage_->state);
  EXPECT(std::fabs(p1_before - 99.7f) < 3.0f, "YC600 power ch1 before the night %.1f W (expected ~99.7)", p1_before);
  EXPECT(std::fabs(p2_before - 49.9f) < 3.0f, "YC600 power ch2 before the night %.1f W (expected ~49.9)", p2_before);
  EXPECT(inv1->energy_total_wh_ > 1.0f, "YC600 accumulated energy %.2f Wh", inv1->energy_total_wh_);
  EXPECT(inv2->ac_voltage_ && inv2->ac_voltage_->has_state() && std::fabs(inv2->ac_voltage_->state - 231.1f) < 0.3f, "DS3 AC voltage (reassembled from 2 fragments) %.1f V", inv2->ac_voltage_->state);
  EXPECT(inv2->power_total_->has_state() && inv2->power_total_->state >= 0.0f, "DS3 total power %.1f W", inv2->power_total_->state);
  EXPECT(inv1->max_power_number_->has_state() && std::fabs(inv1->max_power_number_->state - 300.0f) < 1.0f || did_throttle, "max power number read back: %.0f W", inv1->max_power_number_->state);
  EXPECT(!inv1->online_state_, "YC600 reported offline after the night (online=%d)", inv1->online_state_);
  EXPECT(inv1->power_total_->state == 0.0f, "YC600 power zeroed when offline (%.1f)", inv1->power_total_->state);
  printf("\n%s (%d failure(s))\n", failures ? "SIMULATION FAILED" : "SIMULATION PASSED", failures);
  return failures ? 1 : 0;
}
