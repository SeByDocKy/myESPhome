// Host-side unit tests for the S21 protocol layer.
//
//   g++ -std=c++17 -Wall -Wextra -I.. test_s21_protocol.cpp -o test_s21 && ./test_s21

#include <cstdio>
#include <cstdlib>

#include "s21_protocol.h"

using namespace esphome::faikin::s21;

static int failures = 0;

#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

#define CHECK_NEAR(a, b) CHECK(fabsf((a) - (b)) < 1e-4f)

static ParseStatus feed_all(FrameParser &p, const uint8_t *buf, size_t n) {
  ParseStatus s = ParseStatus::NEED_MORE;
  for (size_t i = 0; i < n; i++)
    s = p.feed(buf[i]);
  return s;
}

static void test_checksum_escape() {
  CHECK(escape_checksum(0x02) == 0x04);
  CHECK(escape_checksum(0x03) == 0x05);
  CHECK(escape_checksum(0x06) == 0x08);
  CHECK(escape_checksum(0x15) == 0x17);
  CHECK(escape_checksum(0x00) == 0x00);
  CHECK(escape_checksum(0x77) == 0x77);
  CHECK(escape_checksum(0xFF) == 0xFF);
}

static void test_build_known_frames() {
  uint8_t out[MAX_FRAME];

  // Wiki example "0246317703": F1 query, checksum 0x77.
  size_t n = build_frame('F', '1', nullptr, 0, out);
  const uint8_t f1[] = {0x02, 0x46, 0x31, 0x77, 0x03};
  CHECK(n == sizeof(f1));
  CHECK(memcmp(out, f1, sizeof(f1)) == 0);

  // Wiki example "02443431A903": D4 with payload "1", checksum 0xA9.
  const uint8_t one = '1';
  n = build_frame('D', '4', &one, 1, out);
  const uint8_t d4[] = {0x02, 0x44, 0x34, 0x31, 0xA9, 0x03};
  CHECK(n == sizeof(d4));
  CHECK(memcmp(out, d4, sizeof(d4)) == 0);

  // Too long a payload is refused.
  uint8_t big[MAX_PAYLOAD + 1] = {0};
  CHECK(build_frame('D', '1', big, sizeof(big), out) == 0);
}

static void test_parser() {
  FrameParser p;

  // Wiki example "02473130334C416803": G1 response, payload "03LA".
  const uint8_t g1[] = {0x02, 0x47, 0x31, 0x30, 0x33, 0x4C, 0x41, 0x68, 0x03};
  CHECK(feed_all(p, g1, sizeof(g1)) == ParseStatus::COMPLETE);
  CHECK(p.validate() == FrameError::NONE);
  CHECK(p.cmd1() == 'G' && p.cmd2() == '1');
  CHECK(p.payload_length() == 4);
  CHECK(memcmp(p.payload(), "03LA", 4) == 0);

  // Garbage before STX is skipped.
  p.reset();
  const uint8_t junk[] = {0x00, 0xFF, 0x06, 0x02, 0x46, 0x31, 0x77, 0x03};
  CHECK(feed_all(p, junk, sizeof(junk)) == ParseStatus::COMPLETE);
  CHECK(p.validate() == FrameError::NONE);
  CHECK(p.payload_length() == 0);

  // Corrupted checksum.
  p.reset();
  const uint8_t bad[] = {0x02, 0x47, 0x31, 0x30, 0x33, 0x4C, 0x41, 0x69, 0x03};
  CHECK(feed_all(p, bad, sizeof(bad)) == ParseStatus::COMPLETE);
  CHECK(p.validate() == FrameError::BAD_CHECKSUM);

  // Runaway frame with no ETX.
  p.reset();
  p.feed(STX);
  ParseStatus s = ParseStatus::NEED_MORE;
  for (size_t i = 0; i < MAX_FRAME + 4 && s == ParseStatus::NEED_MORE; i++)
    s = p.feed('A');
  CHECK(s == ParseStatus::TOO_LONG);
}

static void test_values() {
  // Temperature: '@' is 18.0, 'L' is 24.0, 'X' is 30.0.
  CHECK_NEAR(decode_target_temp('@'), 18.0f);
  CHECK_NEAR(decode_target_temp('L'), 24.0f);
  CHECK_NEAR(decode_target_temp('X'), 30.0f);
  CHECK(encode_target_temp(18.0f) == '@');
  CHECK(encode_target_temp(24.0f) == 'L');
  CHECK(encode_target_temp(30.0f) == 'X');
  CHECK(encode_target_temp(24.3f) == 'L' + 1 || encode_target_temp(24.3f) == 'L');
  CHECK(encode_target_temp(10.0f) == '@');  // clamped
  CHECK(encode_target_temp(40.0f) == 'X');  // clamped
  CHECK(std::isnan(decode_target_temp(0x00)));
  for (float t = 18.0f; t <= 30.0f; t += 0.5f)
    CHECK_NEAR(decode_target_temp(encode_target_temp(t)), t);

  // Fan.
  Fan f;
  CHECK(decode_fan('A', f) && f == Fan::AUTO);
  CHECK(decode_fan('B', f) && f == Fan::QUIET);
  CHECK(decode_fan('3', f) && f == Fan::SPEED_1);
  CHECK(decode_fan('7', f) && f == Fan::SPEED_5);
  CHECK(!decode_fan('9', f));
  CHECK(encode_fan(Fan::AUTO) == 'A');
  CHECK(encode_fan(Fan::QUIET) == 'B');
  CHECK(encode_fan(Fan::SPEED_1) == '3');
  CHECK(encode_fan(Fan::SPEED_5) == '7');

  // Mode.
  Mode m;
  CHECK(decode_mode('0', m) && m == Mode::AUTO);
  CHECK(decode_mode('1', m) && m == Mode::AUTO);
  CHECK(decode_mode('7', m) && m == Mode::AUTO);
  CHECK(decode_mode('2', m) && m == Mode::DRY);
  CHECK(decode_mode('3', m) && m == Mode::COOL);
  CHECK(decode_mode('4', m) && m == Mode::HEAT);
  CHECK(decode_mode('6', m) && m == Mode::FAN);
  CHECK(!decode_mode('5', m));

  // Demand: 100 % is '0', 0x30 + (100 - percent).
  CHECK(encode_demand(100) == '0');
  CHECK(encode_demand(95) == 0x30 + 5);
  CHECK(encode_demand(40) == 0x30 + 60);
  CHECK(encode_demand(150) == '0');  // clamped
  int pct;
  CHECK(decode_demand('0', pct) && pct == 100);
  CHECK(decode_demand(0x30 + 5, pct) && pct == 95);
  CHECK(decode_demand(0x30 + 60, pct) && pct == 40);
  CHECK(!decode_demand(0x20, pct));
  for (int v = 0; v <= 100; v += 5) {
    CHECK(decode_demand(encode_demand(v), pct) && pct == v);
  }
}

static void test_apply_responses() {
  State st;

  // G1 from the wiki example: power off, cool, 24.0, fan auto.
  CHECK(apply_response(st, 'G', '1', (const uint8_t *) "03LA", 4) == GROUP_D1);
  CHECK(!st.power);
  CHECK(st.mode == Mode::COOL);
  CHECK_NEAR(st.target_temp, 24.0f);
  CHECK(st.fan == Fan::AUTO);
  CHECK(st.known & GROUP_D1);

  // Powered on, heat, 21.5, fan level 3 ('5').
  const uint8_t g1b[] = {'1', '4', (uint8_t)('@' + 7), '5'};
  CHECK(apply_response(st, 'G', '1', g1b, 4) == GROUP_D1);
  CHECK(st.power && st.mode == Mode::HEAT);
  CHECK_NEAR(st.target_temp, 21.5f);
  CHECK(st.fan == Fan::SPEED_3);

  // Dry mode keeps the last known target temperature.
  const uint8_t g1c[] = {'1', '2', 0x00, 'A'};
  apply_response(st, 'G', '1', g1c, 4);
  CHECK(st.mode == Mode::DRY);
  CHECK_NEAR(st.target_temp, 21.5f);

  // Too short.
  CHECK(apply_response(st, 'G', '1', g1c, 3) == 0);

  // Quiet fan reported as auto stays quiet while running slowly, goes back to auto when fast.
  st.fan = Fan::QUIET;
  st.power = true;
  st.fan_rpm = 400;
  const uint8_t g1d[] = {'1', '3', 'L', 'A'};
  apply_response(st, 'G', '1', g1d, 4);
  CHECK(st.fan == Fan::QUIET);
  st.fan_rpm = 900;
  apply_response(st, 'G', '1', g1d, 4);
  CHECK(st.fan == Fan::AUTO);

  // Swing.
  const uint8_t g5[] = {'3', '?', '0', '0'};
  CHECK(apply_response(st, 'G', '5', g5, 4) == GROUP_D5);
  CHECK(st.swing_v && st.swing_h);

  // G6 flags: powerful (0x02) | comfort (0x40) in byte 0, streamer in byte 1, sensor LED in byte 3.
  const uint8_t g6[] = {(uint8_t)('0' + 0x02 + 0x40), (uint8_t)('0' + 0x80), '0', (uint8_t)('0' + 0x08)};
  CHECK(apply_response(st, 'G', '6', g6, 4) == GROUP_D6);
  CHECK(st.powerful && st.comfort && !st.quiet && st.streamer && st.sensor_led);

  // G7: demand 70 % with econo on.
  const uint8_t g7[] = {encode_demand(70), '2', '0', '0'};
  CHECK(apply_response(st, 'G', '7', g7, 4) == GROUP_D7);
  CHECK(st.econo && st.demand == 70);
  // Unit without demand control reports '0' -> 100 %.
  const uint8_t g7b[] = {'0', '0', '0', '0'};
  apply_response(st, 'G', '7', g7b, 4);
  CHECK(!st.econo && st.demand == 100);

  // Sensors, values taken from a published S21 log: SH=522+ SI=022+ Sa=532+ SL=000.
  CHECK(apply_response(st, 'S', 'H', (const uint8_t *) "522+", 4) == GROUP_SENSORS);
  CHECK_NEAR(st.home, 22.5f);
  apply_response(st, 'S', 'a', (const uint8_t *) "532+", 4);
  CHECK_NEAR(st.outside, 23.5f);
  apply_response(st, 'S', 'I', (const uint8_t *) "022+", 4);
  CHECK_NEAR(st.liquid, 22.0f);
  apply_response(st, 'S', 'L', (const uint8_t *) "000", 3);
  CHECK(st.fan_rpm == 0);
  apply_response(st, 'S', 'L', (const uint8_t *) "430", 3);
  CHECK(st.fan_rpm == 340);  // "430" -> 4 + 3*10 + 0*100 = 34 -> 340 rpm
  apply_response(st, 'S', 'a', (const uint8_t *) "512-", 4);
  CHECK_NEAR(st.outside, -21.5f);
  // Garbage digits are refused.
  CHECK(apply_response(st, 'S', 'H', (const uint8_t *) "5x2+", 4) == 0);

  // F9 fallback: 0x80 + 2 * degC.
  const uint8_t g9[] = {(uint8_t)(0x80 + 2 * 21), (uint8_t)(0x80 + 2 * 12), '0', '0'};
  CHECK(apply_response(st, 'G', '9', g9, 4) == GROUP_SENSORS);
  CHECK_NEAR(st.home, 21.0f);
  CHECK_NEAR(st.outside, 12.0f);

  // Unknown commands.
  CHECK(apply_response(st, 'G', 'Z', g9, 4) == 0);
  CHECK(apply_response(st, 'X', '1', g9, 4) == 0);
}

static void test_build_payloads() {
  State d;
  uint8_t out[4];

  d.power = true;
  d.mode = Mode::COOL;
  d.target_temp = 24.0f;
  d.fan = Fan::SPEED_2;
  build_d1(d, out);
  CHECK(out[0] == '1' && out[1] == '3' && out[2] == 'L' && out[3] == '4');

  d.mode = Mode::DRY;  // no temperature in dry mode
  build_d1(d, out);
  CHECK(out[1] == '2' && out[2] == '@');

  d.power = false;
  d.mode = Mode::HEAT;
  d.fan = Fan::QUIET;
  build_d1(d, out);
  CHECK(out[0] == '0' && out[1] == '4' && out[3] == 'B');

  d.swing_v = true;
  d.swing_h = false;
  build_d5(d, out);
  CHECK(out[0] == '1' && out[1] == '?');
  d.swing_h = true;
  build_d5(d, out);
  CHECK(out[0] == '7');
  d.swing_v = d.swing_h = false;
  build_d5(d, out);
  CHECK(out[0] == '0' && out[1] == '0');

  d.powerful = true;
  d.quiet = true;
  d.sensor_led = true;
  build_d6(d, out);
  CHECK(out[0] == (uint8_t)('0' + 0x02 + 0x80));
  CHECK(out[1] == '0' && out[3] == (uint8_t)('0' + 0x08));

  // Econo keeps the demand limit, and unknown demand means "no limit".
  d.econo = true;
  d.demand = 60;
  build_d7(d, out);
  CHECK(out[0] == encode_demand(60) && out[1] == '2');
  d.demand = -1;
  build_d7(d, out);
  CHECK(out[0] == '0');

  // Round trip through the parser: build a D7 frame and read it back.
  d.demand = 45;
  d.econo = false;
  build_d7(d, out);
  uint8_t frame[MAX_FRAME];
  const size_t n = build_frame('D', '7', out, 4, frame);
  FrameParser p;
  CHECK(feed_all(p, frame, n) == ParseStatus::COMPLETE);
  CHECK(p.validate() == FrameError::NONE);
  CHECK(p.payload_length() == 4 && p.payload()[0] == encode_demand(45));
}

static void test_state_helpers() {
  State a, b;
  CHECK(state_equal(a, b));  // NaN fields compare equal
  b.home = 21.0f;
  CHECK(!state_equal(a, b));
  a.home = 21.0f;
  CHECK(state_equal(a, b));

  State src;
  src.power = true;
  src.mode = Mode::HEAT;
  src.econo = true;
  src.demand = 80;
  src.known = GROUP_D1 | GROUP_D7;
  State dst;
  copy_groups(dst, src, GROUP_D7);
  CHECK(dst.econo && dst.demand == 80 && !dst.power);
  CHECK(dst.known == GROUP_D7);
  copy_groups(dst, src, GROUP_D1);
  CHECK(dst.power && dst.mode == Mode::HEAT);
}

static void test_probe_helpers() {
  char out[16];
  // "0230" reversed is "0320" -> 3.20 (wiki: FY00 '0230' = 3.20)
  CHECK(format_protocol_version((const uint8_t *) "0230", 4, out, sizeof(out)));
  CHECK(strcmp(out, "3.20") == 0);
  // Selector echoed in front: only the last four bytes count.
  CHECK(format_protocol_version((const uint8_t *) "000430", 6, out, sizeof(out)));
  CHECK(strcmp(out, "3.40") == 0);  // "0430" -> "0340"
  CHECK(!format_protocol_version((const uint8_t *) "ab", 2, out, sizeof(out)));
  CHECK(!format_protocol_version((const uint8_t *) "02x0", 4, out, sizeof(out)));

  char r[8];
  reverse_ascii((const uint8_t *) "ABCD", 4, r);
  CHECK(strcmp(r, "DCBA") == 0);
  const uint8_t nonprint[] = {'A', 0x01, 'B'};
  reverse_ascii(nonprint, 3, r);
  CHECK(strcmp(r, "B.A") == 0);
}

int main() {
  test_checksum_escape();
  test_build_known_frames();
  test_parser();
  test_values();
  test_apply_responses();
  test_build_payloads();
  test_state_helpers();
  test_probe_helpers();
  if (failures == 0)
    printf("All S21 protocol tests passed.\n");
  else
    printf("%d check(s) failed.\n", failures);
  return failures == 0 ? 0 : 1;
}
