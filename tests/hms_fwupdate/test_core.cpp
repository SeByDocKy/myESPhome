// Host tests of components/hms_fwupdate/fwupdate_core.h (no ESPHome needed).
//   g++ -std=c++17 -Wall -Wextra -I../../components/hms_fwupdate test_core.cpp -o test_core && ./test_core
//   ./test_core dump <file.hex> <inverter_sn_hex> <dtu_sn_hex>   -> frames in the reference implementation's log layout
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "fwupdate_core.h"

using namespace esphome::hms_fwupdate;

static int failures = 0;
#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
      failures++;                                                            \
    }                                                                        \
  } while (0)

static void to_id(uint8_t out[4], uint64_t serial) {
  out[0] = serial >> 24;
  out[1] = serial >> 16;
  out[2] = serial >> 8;
  out[3] = serial;
}

static int dump(const char *path, uint64_t inv, uint64_t dtu) {
  std::ifstream f(path);
  if (!f) {
    std::printf("cannot open %s\n", path);
    return 2;
  }
  uint8_t inv_id[4], dtu_id[4];
  to_id(inv_id, inv);
  to_id(dtu_id, dtu);
  std::string line;
  uint8_t row[256];
  FwPacket pk[FW_MAX_PACKETS];
  unsigned rown = 0;
  while (std::getline(f, line)) {
    size_t n = 0;
    HexResult r = fw_decode_hex_line(line.c_str(), line.size(), row, sizeof(row), n, true);
    if (r == HexResult::SKIP) continue;
    if (r == HexResult::ERROR) {
      std::printf("ERROR at row %u\n", rown + 1);
      return 1;
    }
    rown++;
    size_t np = fw_packetize_row(row, n, pk, FW_MAX_PACKETS);
    for (size_t i = 0; i < np; i++) {
      uint8_t frame[32];
      uint8_t len = fw_build_frame(inv_id, dtu_id, pk[i], row, n, frame);
      char hex[3 * 32 + 1];
      fw_hex_dump(frame, len, hex, sizeof(hex));
      std::printf("row %u pkt %zu/%zu ack=%d: %s\n", rown, i + 1, np, fw_packet_expects_ack(pk[i]) ? 1 : 0, hex);
    }
    if (r == HexResult::END_OF_FILE) break;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 5 && std::string(argv[1]) == "dump")
    return dump(argv[2], std::strtoull(argv[3], nullptr, 16), std::strtoull(argv[4], nullptr, 16));

  // CRC16 Modbus check value
  const uint8_t nine[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK(fw_crc16(nine, 9) == 0x4B37);

  // Intel HEX: example header line from veganmosfet/Balcony_in_the_cloud
  {
    const char *l = ":10F80000010800000508A05A99730109545F1714F4";
    uint8_t out[64];
    size_t n = 0;
    CHECK(fw_decode_hex_line(l, strlen(l), out, sizeof(out), n, true) == HexResult::DATA);
    CHECK(n == 20);
    CHECK(out[0] == 0x10 && out[1] == 0xF8 && out[2] == 0x00 && out[3] == 0x00 && out[4] == 0x01);
    // corrupted data byte: strict refuses, lenient accepts
    const char *bad = ":10F80000010800000508A05A99730109545F1715F4";
    CHECK(fw_decode_hex_line(bad, strlen(bad), out, sizeof(out), n, true) == HexResult::ERROR);
    CHECK(fw_decode_hex_line(bad, strlen(bad), out, sizeof(out), n, false) == HexResult::DATA);
    // wrong declared length
    const char *badlen = ":0FF80000010800000508A05A99730109545F1714F5";
    CHECK(fw_decode_hex_line(badlen, strlen(badlen), out, sizeof(out), n, true) == HexResult::ERROR);
    // EOF, blank, comment, CRLF, odd length, not hex
    CHECK(fw_decode_hex_line(":00000001FF\r\n", 13, out, sizeof(out), n, true) == HexResult::END_OF_FILE);
    CHECK(n == 4);
    CHECK(fw_decode_hex_line("", 0, out, sizeof(out), n, true) == HexResult::SKIP);
    CHECK(fw_decode_hex_line("# x", 3, out, sizeof(out), n, true) == HexResult::SKIP);
    CHECK(fw_decode_hex_line(":10F8000", 8, out, sizeof(out), n, true) == HexResult::ERROR);
    CHECK(fw_decode_hex_line(":ZZ00000001FF", 13, out, sizeof(out), n, true) == HexResult::ERROR);
    // output buffer too small
    uint8_t tiny[4];
    CHECK(fw_decode_hex_line(l, strlen(l), tiny, sizeof(tiny), n, true) == HexResult::ERROR);
  }

  // Packetization: where the row CRC goes
  {
    uint8_t row[256];
    for (int i = 0; i < 256; i++) row[i] = (uint8_t) i;
    FwPacket pk[FW_MAX_PACKETS];
    struct Case { size_t len; size_t np; uint8_t last_number; uint8_t last_len; };
    const Case cases[] = {
        {4, 1, 0x81, 4},     // EOF-like row
        {20, 2, 0x82, 4},    // 16 + 4, CRC fits
        {30, 2, 0x82, 14},   // 16 + 14, CRC fits exactly
        {31, 3, 0x83, 0},    // 16 + 15: CRC goes into its own packet
        {32, 3, 0x83, 0},    // 16 + 16
        {36, 3, 0x83, 4},    // 16 + 16 + 4
        {255, 17, 0x91, 15}, // largest row: 15 full chunks + 15 bytes -> CRC does not fit -> extra packet
    };
    for (const auto &c : cases) {
      size_t np = fw_packetize_row(row, c.len, pk, FW_MAX_PACKETS);
      // the 255-byte case is the one where the extra CRC packet appears
      size_t expect_np = c.len == 255 ? 17 : c.np;
      CHECK(np == expect_np);
      if (np) {
        // only the last packet expects an ack, and it carries the CRC
        for (size_t i = 0; i + 1 < np; i++) CHECK(!fw_packet_expects_ack(pk[i]));
        CHECK(fw_packet_expects_ack(pk[np - 1]));
        CHECK(pk[np - 1].has_crc);
        // payload slices re-assemble to the row
        uint8_t re[300];
        size_t pos = 0;
        for (size_t i = 0; i < np; i++) {
          memcpy(re + pos, pk[i].data, pk[i].data_len);
          pos += pk[i].data_len;
        }
        CHECK(pos == c.len && memcmp(re, row, c.len) == 0);
        if (c.len != 255) {
          CHECK(pk[np - 1].number == c.last_number);
          CHECK(pk[np - 1].data_len == c.last_len);
        }
      }
    }
    CHECK(fw_packetize_row(row, 0, pk, FW_MAX_PACKETS) == 0);
    CHECK(fw_packetize_row(row, 256, pk, FW_MAX_PACKETS) == 0);
    CHECK(fw_packetize_row(row, 100, pk, 3) == 0);  // output array too small
  }

  // Frame layout: header, packet number, payload, big-endian CRC16, XOR checksum
  {
    uint8_t row[] = {0x10, 0xF8, 0x00, 0x00, 1, 2, 3, 4};  // 8 bytes
    FwPacket pk[FW_MAX_PACKETS];
    size_t np = fw_packetize_row(row, sizeof(row), pk, FW_MAX_PACKETS);
    CHECK(np == 1);
    const uint8_t inv[4] = {0x71, 0x60, 0x35, 0x46};
    const uint8_t dtu[4] = {0x80, 0x12, 0x23, 0x04};
    uint8_t f[32];
    uint8_t len = fw_build_frame(inv, dtu, pk[0], row, sizeof(row), f);
    CHECK(len == 10 + 8 + 2 + 1);
    CHECK(f[0] == 0x0E && f[9] == 0x81);
    CHECK(memcmp(&f[1], inv, 4) == 0 && memcmp(&f[5], dtu, 4) == 0);
    CHECK(memcmp(&f[10], row, 8) == 0);
    const uint16_t crc = fw_crc16(row, sizeof(row));
    CHECK(f[18] == (crc >> 8) && f[19] == (crc & 0xFF));
    uint8_t x = 0;
    for (int i = 0; i < len - 1; i++) x ^= f[i];
    CHECK(f[len - 1] == x);
    // the whole frame XORs to 0
    x = 0;
    for (int i = 0; i < len; i++) x ^= f[i];
    CHECK(x == 0);

    // acknowledgement: command 0x8E, payload starts with the first 4 row bytes
    uint8_t ack[16] = {0x8E, 0x71, 0x60, 0x35, 0x46, 0x80, 0x12, 0x23, 0x04, 0x81, 0x10, 0xF8, 0x00, 0x00, 0x00};
    CHECK(fw_is_row_ack(ack, 15, row, sizeof(row)));
    ack[11] = 0xF9;  // another row's address
    CHECK(!fw_is_row_ack(ack, 15, row, sizeof(row)));
    ack[11] = 0xF8;
    ack[0] = 0x15;  // not a download answer
    CHECK(!fw_is_row_ack(ack, 15, row, sizeof(row)));
    ack[0] = 0x8E;
    CHECK(!fw_is_row_ack(ack, 12, row, sizeof(row)));  // payload shorter than 4 bytes
  }

  // Serial rules + identity row
  {
    CHECK(fw_find_rule(0x114300000001ULL) != nullptr);
    CHECK(fw_find_rule(0x152000000001ULL) == nullptr);  // MIT-5000: rule exists but it is not allowed
    CHECK(fw_find_rule(0x999900000001ULL) == nullptr);
    const FwSerialRule *r = fw_find_rule(0x114300000001ULL);  // HMS 2T B: gen1 0, phase 1, in 1, dsp 1, gen2 0, gen3 0, btype 1
    uint8_t row[12] = {0x10, 0xF8, 0x00, 0x00, 0xF0, 0x11, 0x10, 0x01, 0, 0, 0, 0};
    CHECK(fw_identity_matches(*r, row, 12));  // high nibble of byte [4] is ignored
    row[5] = 0x21;                            // wrong phase
    CHECK(!fw_identity_matches(*r, row, 12));
    row[5] = 0x11;
    CHECK(!fw_identity_matches(*r, row, 7));  // too short
    CHECK(fw_ack_resends(0x00) == 10 && fw_ack_resends(0x01) == 17 && fw_ack_resends(0x04) == 23);
  }

  if (failures == 0) std::printf("all tests passed\n");
  return failures ? 1 : 0;
}
