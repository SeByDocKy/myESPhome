// Host simulation of the apszb hub in "radio: znp" mode: a fake CC2530 (Z-Stack ZNP firmware) is attached to a pseudo
// terminal, which the ESPHome host UART opens (/tmp/apszb_sim_tty). A fake YC600 (0x103A) and a fake DS3 (pairs as 0x6C22)
// live behind it. Unlike the native radio, Z-Stack delivers complete (already reassembled) messages.
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
#include <csignal>
#include <cstdarg>
#include <climits>
#include <algorithm>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
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
static std::string tohex(const uint8_t *d, size_t n) {
  std::string s; char b[4];
  for (size_t i = 0; i < n; i++) { snprintf(b, sizeof(b), "%02X", d[i]); s += b; }
  return s;
}

// ---------------------------------------------------------------------------------------------------------------------
// fake inverters (same data as the native radio simulation)
// ---------------------------------------------------------------------------------------------------------------------
static std::vector<uint8_t> yc_answer(int k) {
  std::string s = "408000158215FBFB51B103D40F411700007 4CF000000767 06A73D06B0496";
  std::string t; for (char c : s) if (c != ' ') t += c;
  auto v = hex(t); v.resize(60, 0);
  uint16_t ts = 1000 + 5 * k;           v[17] = ts >> 8; v[18] = ts & 0xFF;
  uint32_t e0 = 1000 + 60 * k, e1 = 500 + 30 * k;
  v[37] = e0 >> 16; v[38] = e0 >> 8; v[39] = e0; v[42] = e1 >> 16; v[43] = e1 >> 8; v[44] = e1;
  return v;
}
static std::vector<uint8_t> ds3_answer(int k) {
  auto v = hex("703000021300FBFB5CBBBB2000FC0001FFFF000000000000000006E506EE015901DA036E13882BBB01480026FFFF052508430"
               "03A40B20035385200");
  std::vector<uint8_t> w(v.begin(), v.end());
  w.resize(66, 0);
  uint16_t ts = 2000 + 5 * k; w[38] = ts >> 8; w[39] = ts & 0xFF;
  uint32_t e0 = 100000 + 6000 * k;
  w[50] = e0 >> 24; w[51] = e0 >> 16; w[52] = e0 >> 8; w[53] = e0;
  return w;
}
static std::vector<uint8_t> yc_query_answer() {
  return hex("408000158215FBFB4DDE041105440FE5020F32B003CF05440FE5020F32B0066604CC0EA3A804D70214050C100FD80ED07A0F32B0056A054F"
             "019000641F3FE480068ACE8ACE000130103030190604001D21DB3B6600000000FEFE");
}

// ---------------------------------------------------------------------------------------------------------------------
// fake CC2530
// ---------------------------------------------------------------------------------------------------------------------
struct FakeModem {
  int fd = -1;
  ZnpParser parser;
  std::vector<std::string> init_log;          // hex of every command received before the coordinator started
  bool started = false;
  double start_at = -1;                        // when to send ZDO_STATE_CHANGE_IND
  bool night = false;
  int yc_polls = 0, ds_polls = 0, queries = 0, pair_cmds = 0, normal_ops = 0, af_reqs = 0;
  std::vector<std::string> first_poll_frame;   // hex of the first AF_DATA_REQUEST to the YC600
  std::string pair_frames[4];
  uint8_t tid = 0;

  void send(uint8_t c0, uint8_t c1, const std::vector<uint8_t> &d) {
    uint8_t buf[260];
    size_t n = znp_encode(c0, c1, d.data(), d.size(), buf);
    (void) !write(fd, buf, n);
  }
  void incoming(uint16_t src, uint16_t cluster, const std::vector<uint8_t> &payload) {
    std::vector<uint8_t> d = {0, 0, uint8_t(cluster), uint8_t(cluster >> 8), uint8_t(src), uint8_t(src >> 8), 0x14, 0x14, 0, 0x71, 0,
                              0xB5, 0x7C, 0xFA, 0, tid++, uint8_t(payload.size())};
    d.insert(d.end(), payload.begin(), payload.end());
    send(0x44, 0x81, d);
  }
  void on_frame(const ZnpFrame &f, double now) {
    std::string h = tohex(&f.cmd0, 1) + tohex(&f.cmd1, 1) + tohex(f.data, f.len);
    if (!started && !(f.cmd0 == 0x24 && f.cmd1 >= 1)) init_log.push_back(h);
    if (f.cmd0 == 0x41 && f.cmd1 == 0x00) {              // SYS_RESET_REQ -> SYS_RESET_IND only
      send(0x41, 0x80, {0x00, 0x02, 0x00, 0x02, 0x06, 0x03});
      return;
    }
    if ((f.cmd0 & 0xE0) != 0x20) return;
    if (f.cmd0 == 0x26 && f.cmd1 == 0x00) {              // ZB_START_REQUEST
      send(0x66, 0x00, {});
      started = true; start_at = now + 0.3;
      return;
    }
    if (f.cmd0 == 0x24 && (f.cmd1 == 0x01 || f.cmd1 == 0x02)) {
      af_reqs++;
      send(0x64, f.cmd1, {0x00});
      uint16_t cluster, dst = 0xFFFF; size_t off;
      if (f.cmd1 == 0x01) { dst = f.data[0] | (f.data[1] << 8); cluster = f.data[4] | (f.data[5] << 8); off = 10; }
      else                { cluster = f.data[13] | (f.data[14] << 8); off = 20; }   // 0F + 8xFF + EP + PAN(2) + EP, then cluster
      if (f.cmd1 == 0x01) send(0x44, 0x80, {0x00, 0x14, f.data[6]});
      const uint8_t *a = f.data + off;
      size_t n = f.len - off;
      if (f.cmd1 == 0x02) {
        // pairing: remember the four broadcasts, answer step 1 (cluster 0x020C) for the DS3
        for (int i = 0; i < 4; i++) {
          static const uint16_t cl[4] = {0x020D, 0x020C, 0x010F, 0x0101};
          if (cluster == cl[i]) pair_frames[i] = tohex(f.data, f.len);
        }
        if (cluster == 0x020C) {
          pair_cmds++;
          if (!night && memcmp(a, hex("703000021300").data(), 6) == 0) {
            std::vector<uint8_t> r(a, a + 6); r.push_back(0x22); r.push_back(0x6C);
            incoming(0x6C22, 0x0106, r);
          }
        }
        return;
      }
      if (cluster == 0x0006 && dst == 0xFFFF) { normal_ops++; return; }
      if (night) return;
      bool is_poll = n >= 12 && a[6] == 0xFB && a[8] == 0x06 && a[9] == 0xBB;
      bool is_query = n >= 12 && a[6] == 0xFB && a[8] == 0x06 && a[9] == 0xDE;
      if (dst == 0x103A) {
        if (is_poll) { if (first_poll_frame.empty()) first_poll_frame.push_back(h); incoming(0x103A, 0x0106, yc_answer(yc_polls++)); }
        if (is_query) { queries++; incoming(0x103A, 0x0106, yc_query_answer()); }
      } else if (dst == 0x6C22) {
        if (is_poll) incoming(0x6C22, 0x0106, ds3_answer(ds_polls++));
      }
      return;
    }
    // every other SREQ (ZB_WRITE_CONFIGURATION, AF_REGISTER...): success
    send(f.cmd0 | 0x40, f.cmd1, {0x00});
  }
  void poll(double now) {
    if (start_at >= 0 && now >= start_at) { start_at = -1; send(0x45, 0xC0, {0x09}); }
    uint8_t b[256];
    ssize_t n = read(fd, b, sizeof(b));
    for (ssize_t i = 0; i < n; i++) {
      ZnpFrame f;
      if (parser.feed(b[i], uint32_t(now * 1000), f)) on_frame(f, now);
    }
  }
};

namespace esphome { namespace host { void setup_preferences(); } }
int main() {
  FakeModem modem;
  modem.fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
  grantpt(modem.fd); unlockpt(modem.fd);
  unlink("/tmp/apszb_sim_tty");
  if (symlink(ptsname(modem.fd), "/tmp/apszb_sim_tty") != 0) { perror("symlink"); return 2; }
  { struct termios t; tcgetattr(modem.fd, &t); cfmakeraw(&t); tcsetattr(modem.fd, TCSANOW, &t); }

  esphome::host::setup_preferences();
  setup();
  auto t0 = Clock::now();
  auto now_s = [&]() { return std::chrono::duration<double>(Clock::now() - t0).count(); };
  // output platform: a burst of writes must end up as ONE queued throttle operation (the latest value, 50 % of 600 W)
  apszb_limit->set_level(0.2f);
  apszb_limit->set_level(0.5f);
  apszb_limit->set_level(0.5f);
  int thr = 0, thr_arg = 0;
  for (auto &op : aps->ops_) if (op.kind == OpKind::THROTTLE && op.inv == 0) { thr++; thr_arg = op.arg; }
  EXPECT(thr == 1 && thr_arg == 300, "output burst coalesced into one throttle of %d W (%d queued)", thr_arg, thr);
  bool night = false, did_query = false, did_throttle = false, paired_seen = false;
  float p1_before = NAN, p2_before = NAN;
  double ready_at = -1;

  while (now_s() < 72.0) {
    loop();
    modem.poll(now_s());
    if (ready_at < 0 && aps->znp_.ready()) { ready_at = now_s(); printf("[t=%.1fs] ZNP coordinator ready\n", ready_at); }
    if (!did_query && now_s() > 22) { did_query = true; printf("[t=%.0fs] query max power\n", now_s()); inv1->request_query(); }
    if (!did_throttle && now_s() > 26) { did_throttle = true; printf("[t=%.0fs] set max power 250 W\n", now_s()); inv1->request_max_power(250); }
    if (!night && now_s() > 35.5 && std::isnan(p1_before)) { p1_before = inv1->power_[0]->state; p2_before = inv1->power_[1]->state; }
    if (!night && now_s() > 36) { night = true; modem.night = true; printf("[t=%.0fs] NIGHT: inverters stop answering\n", now_s()); }
    if (!paired_seen && inv2->paired()) { paired_seen = true; printf("[t=%.1fs] inv2 paired as 0x%04X\n", now_s(), inv2->short_address()); }
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
  }

  printf("\n=== results ===\n");
  printf("yc polls: %d, ds3 polls: %d, queries: %d, AF requests: %d, normal-op broadcasts: %d\n", modem.yc_polls, modem.ds_polls,
         modem.queries, modem.af_reqs, modem.normal_ops);
  // start-up sequence exactly as in ZIGBEE_COORDINATOR.ino
  // (ZB_WRITE_CONFIGURATION payload = ConfigId, Len, Value; the 8th command is ZB_START_REQUEST)
  std::vector<std::string> got = modem.init_log;
  EXPECT(got.size() >= 7, "7 configuration commands received before the start request (%zu)", got.size());
  const char *want[7] = {"2605030103", "410000", "26050108FFFF80971B01A3D8", "2605870100", "26058302D8A3", "2605840400000100",
                         "240014050F00010100020000150000"};
  for (int i = 0; i < 7 && i < (int) got.size(); i++) EXPECT(got[i] == want[i], "init %d: %s", i, got[i].c_str());
  EXPECT(ready_at > 0 && ready_at < 8, "coordinator ready after %.1f s", ready_at);
  EXPECT(modem.first_poll_frame.size() == 1 && modem.first_poll_frame[0].rfind("24013A1014140600", 0) == 0 &&
             modem.first_poll_frame[0].find("0F1380971B01A3D8FBFB06BB000000000000C1FEFE") != std::string::npos,
         "YC600 poll = AF_DATA_REQUEST like the original: %s", modem.first_poll_frame.empty() ? "-" : modem.first_poll_frame[0].c_str());
  EXPECT(modem.pair_frames[1].find("24020FFFFFFFFFFFFFFFFF14FFFF140C02") == 0 || modem.pair_frames[1].find("0FFFFFFFFFFFFFFFFF14FFFF140C02") == 0,
         "pairing step 1 = AF_DATA_REQUEST_EXT broadcast: %s", modem.pair_frames[1].c_str());
  EXPECT(inv2->short_address() == 0x6C22, "DS3 auto-paired as 0x6C22 (got 0x%04X)", inv2->short_address());
  EXPECT(modem.pair_cmds >= 1, "pairing broadcasts seen (%d)", modem.pair_cmds);
  EXPECT(modem.yc_polls >= 4, "several YC600 polls answered (%d)", modem.yc_polls);
  EXPECT(modem.ds_polls >= 2, "several DS3 polls answered (%d)", modem.ds_polls);
  EXPECT(modem.normal_ops >= 1, "'normal operation' broadcast sent (%d)", modem.normal_ops);
  EXPECT(inv1->ac_voltage_->has_state() && std::fabs(inv1->ac_voltage_->state - 221.1f) < 0.3f, "YC600 AC voltage %.1f V", inv1->ac_voltage_->state);
  EXPECT(std::fabs(p1_before - 99.7f) < 3.0f, "YC600 power ch1 %.1f W (expected ~99.7)", p1_before);
  EXPECT(std::fabs(p2_before - 49.9f) < 3.0f, "YC600 power ch2 %.1f W (expected ~49.9)", p2_before);
  EXPECT(inv2->ac_voltage_->has_state() && std::fabs(inv2->ac_voltage_->state - 231.1f) < 0.3f, "DS3 AC voltage %.1f V", inv2->ac_voltage_->state);
  EXPECT(inv1->link_quality_ != nullptr && inv1->link_quality_->has_state(), "link quality published (%.0f)", inv1->link_quality_->state);
  EXPECT(inv1->max_power_number_->has_state() && std::fabs(inv1->max_power_number_->state - 300.0f) < 1.0f, "max power read back: %.0f W", inv1->max_power_number_->state);
  EXPECT(!inv1->online_state_, "YC600 offline after the night");
  EXPECT(inv1->power_total_->state == 0.0f, "YC600 power zeroed when offline (%.1f)", inv1->power_total_->state);
  printf("\n%s (%d failure(s))\n", failures ? "SIMULATION FAILED" : "SIMULATION PASSED", failures);
  unlink("/tmp/apszb_sim_tty");
  return failures ? 1 : 0;
}
