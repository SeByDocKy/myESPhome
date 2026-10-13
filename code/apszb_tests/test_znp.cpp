// Unit tests of the ZNP (CC2530) backend: framing, stream parser, start-up sequence, error paths and TX encoding.
// Build: g++ -std=c++17 -I../components/apszb test_znp.cpp ../components/apszb/znp_radio.cpp ../components/apszb/aps_protocol.cpp -o t_znp
#include "znp_radio.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace esphome::apszb;

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: " __VA_ARGS__); printf("  [%s]\n", #cond); fails++; } else { printf("ok:   " __VA_ARGS__); printf("\n"); } } while (0)

static std::string hexs(const uint8_t *d, size_t n) {
  std::string s; char b[4];
  for (size_t i = 0; i < n; i++) { snprintf(b, sizeof(b), "%02X", d[i]); s += b; }
  return s;
}

struct FakePort : ZnpPort {
  std::vector<uint8_t> tx, rx;
  size_t rx_pos = 0;
  std::vector<int> reset_events;
  bool reset_pin = false;
  void write(const uint8_t *d, size_t n) override { tx.insert(tx.end(), d, d + n); }
  int read_byte() override { return rx_pos < rx.size() ? rx[rx_pos++] : -1; }
  void set_reset(bool a) override { reset_events.push_back(a); }
  bool has_reset() const override { return reset_pin; }
  void feed(uint8_t c0, uint8_t c1, const std::vector<uint8_t> &d) {
    uint8_t b[260];
    size_t n = znp_encode(c0, c1, d.data(), d.size(), b);
    rx.insert(rx.end(), b, b + n);
  }
  // split the bytes written by the host into ZNP frames
  std::vector<ZnpFrame> frames() {
    std::vector<ZnpFrame> out; ZnpParser p; ZnpFrame f;
    for (uint8_t b : tx) if (p.feed(b, 0, f)) out.push_back(f);
    tx.clear();
    return out;
  }
};

static NetworkConfig make_cfg() {
  NetworkConfig c; uint8_t id[6] = {0xD8, 0xA3, 0x01, 0x1B, 0x97, 0x80}; c.set_ecu_id(id); return c;
}

static std::vector<std::string> logs;
static void logger(int level, const char *msg) { logs.push_back(std::to_string(level) + ":" + msg); }
static bool has_log(const char *needle) { for (auto &l : logs) if (l.find(needle) != std::string::npos) return true; return false; }

int main() {
  // ---- framing ----------------------------------------------------------------------------------------------------
  {
    uint8_t out[16];
    // "2600" ZB_START_REQUEST -> FE 00 26 00 26 (example of the original log: FE00260026)
    size_t n = znp_encode(0x26, 0x00, nullptr, 0, out);
    CHECK(hexs(out, n) == "FE00260026", "ZB_START_REQUEST frame = %s", hexs(out, n).c_str());
    // "FE0D240014050F0001010002000015000020" is AF_REGISTER of the original log
    const uint8_t reg[] = {0x14, 0x05, 0x0F, 0x00, 0x01, 0x01, 0x00, 0x02, 0x00, 0x00, 0x15, 0x00, 0x00};
    uint8_t big[32];
    n = znp_encode(0x24, 0x00, reg, sizeof(reg), big);
    CHECK(hexs(big, n) == "FE0D240014050F0001010002000015000020", "AF_REGISTER frame = %s", hexs(big, n).c_str());
    // "FE062605840400000100A4": channel list
    const uint8_t ch[] = {0x84, 0x04, 0x00, 0x00, 0x01, 0x00};
    n = znp_encode(0x26, 0x05, ch, sizeof(ch), big);
    CHECK(hexs(big, n) == "FE062605840400000100A4", "channel list frame = %s", hexs(big, n).c_str());
  }
  // ---- parser: noise, bad checksum, truncated frame, back to back frames --------------------------------------------
  {
    ZnpParser p; ZnpFrame f; int got = 0;
    std::vector<uint8_t> s = {0x00, 0x55};                                    // noise
    uint8_t b[16]; size_t n = znp_encode(0x45, 0xC0, (const uint8_t[]){0x09}, 1, b);
    s.insert(s.end(), b, b + n);
    s.insert(s.end(), b, b + n);
    s.back() ^= 0xFF;                                                         // corrupt the second frame's FCS
    s.insert(s.end(), b, b + n);
    uint32_t t = 0;
    for (uint8_t c : s) if (p.feed(c, t++, f)) { got++; CHECK(f.cmd0 == 0x45 && f.cmd1 == 0xC0 && f.data[0] == 9, "parsed ZDO_STATE_CHANGE_IND"); }
    CHECK(got == 2, "2 valid frames out of 3 (%d), bad=%u", got, (unsigned) p.bad_frames());
    // a frame that stalls half way must not block the parser
    ZnpParser q; got = 0;
    q.feed(0xFE, 0, f); q.feed(0x05, 1, f); q.feed(0x24, 2, f);              // truncated
    for (size_t i = 0; i < n; i++) if (q.feed(b[i], 1000 + uint32_t(i), f)) got++;
    CHECK(got == 1, "parser resynchronises after a stalled frame");
  }
  // ---- start-up sequence ----------------------------------------------------------------------------------------------
  {
    FakePort port; port.reset_pin = true;
    ZnpRadio z; z.set_port(&port); z.set_logger(logger); logs.clear();
    z.begin(make_cfg());
    uint32_t now = 1000;
    z.loop(now);
    CHECK(port.reset_events.size() == 1 && port.reset_events[0] == 1, "RESET asserted first");
    now += 60; z.loop(now);
    CHECK(port.reset_events.size() == 2 && port.reset_events[1] == 0, "RESET released after 50 ms");
    now += 600; z.loop(now);   // settle
    now += 5; z.loop(now);     // -> first command
    auto fr = port.frames();
    CHECK(fr.size() == 1 && fr[0].cmd0 == 0x26 && fr[0].cmd1 == 0x05, "first command is ZB_WRITE_CONFIGURATION");
    // answer every command in turn
    int guard = 0;
    while (!z.ready() && guard++ < 40) {
      if (!fr.empty()) {
        for (auto &f : fr) {
          if (f.cmd0 == 0x41) port.feed(0x41, 0x80, {0, 2, 0, 2, 6, 3});
          else if (f.cmd0 == 0x26 && f.cmd1 == 0x00) { port.feed(0x66, 0x00, {0}); port.feed(0x45, 0xC0, {0x09}); }
          else port.feed(f.cmd0 | 0x40, f.cmd1, {0});
        }
      }
      now += 5; z.loop(now); fr = port.frames();
    }
    CHECK(z.ready(), "coordinator ready after the answers");
    CHECK(has_log("2:ZNP init 1/8 startup option: clear state: OK") && has_log("2:ZNP init 3/8 extended address: OK") &&
              has_log("2:ZNP init 7/8 AF_REGISTER: OK") && has_log("2:ZNP init 8/8 ZB_START_REQUEST: accepted"),
          "one INFO line per successful init step");
    CHECK(has_log("ZNP init 2/8 SYS_RESET_REQ: OK (modem restarted)"), "reset indication logged");
    CHECK(has_log("started as ZigBee coordinator"), "start logged");
    // ---- transmit encoding ----
    TxFrame tf; NetworkConfig cfg = make_cfg(); FrameFactory ff(&cfg);
    uint8_t ecu_rev[6]; cfg.ecu_reverse(ecu_rev);
    uint8_t asdu[100]; size_t an = asdu_poll(asdu, ecu_rev);
    ff.build_unicast(tf, 0x103A, CLUSTER_ECU_TO_INV, asdu, an);
    CHECK(z.transmit(tf), "unicast accepted");
    fr = port.frames();
    CHECK(fr.size() == 1 && fr[0].cmd0 == 0x24 && fr[0].cmd1 == 0x01, "AF_DATA_REQUEST sent");
    std::string d = hexs(fr[0].data, fr[0].len);
    CHECK(d.rfind("3A10" "1414" "0600", 0) == 0 && d.find("0F13" "80971B01A3D8FBFB06BB000000000000C1FEFE") != std::string::npos,
          "poll payload = original: %s", d.c_str());
    CHECK(z.tx_result() == -1, "waiting for the SRSP");
    CHECK(!z.transmit(tf), "second transmit refused while busy");
    port.feed(0x64, 0x01, {0x00}); now += 5; z.loop(now);
    CHECK(z.tx_result() == 0, "SRSP success -> tx ok");
    ff.build_unicast(tf, 0x103A, CLUSTER_ECU_TO_INV, asdu, an);
    z.transmit(tf); port.frames(); port.feed(0x64, 0x01, {0x1A}); now += 5; z.loop(now);
    CHECK(z.tx_result() > 0, "SRSP error -> tx failed (%s)", ZnpRadio::tx_error_name(z.tx_result()));
    // pairing broadcast
    uint8_t pa[40]; uint16_t cl; uint8_t ser[6] = {0x40, 0x80, 0x00, 0x15, 0x82, 0x15};
    size_t pn = asdu_pair(pa, 1, cfg, ser, &cl);
    ff.build_broadcast(tf, ADDR_BROADCAST, cl, pa, pn);
    port.tx.clear(); z.transmit(tf); fr = port.frames();
    d = hexs(fr[0].data, fr[0].len);
    CHECK(fr[0].cmd1 == 0x02 && d.rfind("0FFFFFFFFFFFFFFFFF14FFFF140C02", 0) == 0 && d.substr(d.size() - 16) == "0600408000158215",
          "pairing = AF_DATA_REQUEST_EXT (broadcast, PAN 0xFFFF): %s", d.c_str());
    port.feed(0x64, 0x02, {0}); now += 5; z.loop(now);
    // normal operation broadcast in our PAN -> plain AF_DATA_REQUEST to 0xFFFF
    an = asdu_normal_ops(asdu, ecu_rev);
    ff.build_broadcast(tf, cfg.pan_id, CLUSTER_ECU_TO_INV, asdu, an);
    port.tx.clear(); z.transmit(tf); fr = port.frames();
    CHECK(fr[0].cmd1 == 0x01 && hexs(fr[0].data, 6) == "FFFF14140600", "normal operation = AF_DATA_REQUEST to 0xFFFF");
    port.feed(0x64, 0x01, {0}); now += 5; z.loop(now);
    // NWK commands have no message: nothing to send
    TxFrame ls; uint16_t nb[1] = {0x103A}; ff.build_link_status(ls, nb, 1);
    CHECK(!z.transmit(ls), "link status (no ZNP equivalent) is refused");
    // ---- reception ----
    std::vector<uint8_t> in = {0, 0, 0x06, 0x01, 0x3A, 0x10, 0x14, 0x14, 0, 0x71, 0, 0xB5, 0x7C, 0xFA, 0, 0, 6, 1, 2, 3, 4, 5, 6};
    port.feed(0x44, 0x81, in); now += 5; z.loop(now);
    ZnpRx m;
    CHECK(z.pop(m) && m.src == 0x103A && m.cluster == 0x0106 && m.lqi == 0x71 && m.len == 6 && m.data[5] == 6, "AF_INCOMING_MSG decoded");
    in[16] = 9;   // length larger than the frame: must be ignored
    port.feed(0x44, 0x81, in); now += 5; z.loop(now);
    CHECK(!z.pop(m), "truncated AF_INCOMING_MSG ignored");
    // ---- the modem restarts by itself ----
    logs.clear();
    port.feed(0x41, 0x80, {1, 2, 0, 2, 6, 3}); now += 5; z.loop(now);
    CHECK(!z.ready() && has_log("reset by itself"), "unexpected SYS_RESET_IND -> coordinator restarted");
  }
  // ---- no modem connected -------------------------------------------------------------------------------------------
  {
    FakePort port;
    ZnpRadio z; z.set_port(&port); z.set_logger(logger); logs.clear();
    z.begin(make_cfg());
    uint32_t now = 0;
    for (; now < 3000; now += 10) z.loop(now);
    CHECK(!z.ready() && has_log("does not answer"), "silent modem detected and reported");
    CHECK(has_log("starting the CC2530 coordinator"), "start attempt logged");
    uint32_t inits = z.init_count();
    for (; now < 40000; now += 10) z.loop(now);
    CHECK(z.init_count() > inits, "initialisation is retried (%u attempts)", (unsigned) z.init_count());
    TxFrame tf; NetworkConfig cfg = make_cfg(); FrameFactory ff(&cfg);
    uint8_t asdu[10] = {0};
    ff.build_unicast(tf, 0x103A, 6, asdu, 6);
    CHECK(!z.transmit(tf), "no transmission while the modem is not ready");
  }
  printf("\n%s\n", fails ? "SOME TESTS FAILED" : "ALL ZNP TESTS PASSED");
  return fails ? 1 : 0;
}
