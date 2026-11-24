// SPDX-License-Identifier: GPL-2.0-or-later
//
// Protocol core of the hms_fwupdate component. It has NO ESPHome dependency, so it can be
// unit-tested on a PC (see tests/).
//
// Derived from xX-nichtlachen-Xx/OpenDTU, branch FirmwareUpdate_HM_IV
// (GPL-2.0-or-later, Copyright (C) 2022-2026 nichtlachen / Thomas Basler and others):
//   lib/Hoymiles/src/commands/FirmwareCommand.cpp      (frame layout, XOR checksum)
//   lib/Hoymiles/src/commands/FirmwareDataCommand.cpp  (packet numbering, timeouts, resend counts)
//   lib/Hoymiles/src/inverters/HM_Abstract.cpp         (enqueueFirmwareRow: 16-byte chunking + row CRC16)
//   lib/Hoymiles/src/utils/IntelHex.h                  (Intel HEX row decoding)
//   src/WebApi_devinfo.cpp                             (serial prefix rules / identity row check)
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace hms_fwupdate {

// ---------------------------------------------------------------------------
// Protocol constants (FirmwareDataCommand.cpp)
// ---------------------------------------------------------------------------
static const uint8_t FW_CMD_DOWN_PRO = 0x0E;   // the only command id used for the whole download
static const uint8_t FW_CHUNK_SIZE = 16;       // payload bytes per radio packet
static const uint8_t FW_ACK_FLAG = 0x80;       // set in the packet number of the last packet of a row
static const uint8_t FW_CRC_SIZE = 2;
static const size_t FW_MAX_ROW_LEN = 255;      // the row CRC length is a uint8_t in the reference code
static const size_t FW_MAX_PACKETS = 20;       // ceil(255 / 16) + 1 (separate CRC packet)
static const uint32_t FW_GAP_MS = 30;          // wait after a packet that expects no answer
static const uint32_t FW_ACK_TIMEOUT_MS = 350; // wait for the row acknowledgement

/// Number of times the LAST packet of a row is re-sent before the whole row is re-sent
/// (FirmwareDataCommand::rowAckResendCount): 10 for data rows, 17 for the EOF row, 23 otherwise
/// (address / vendor rows that trigger a flash erase).
inline uint8_t fw_ack_resends(uint8_t record_type) {
  switch (record_type) {
    case 0x00:
      return 10;
    case 0x01:
      return 17;
    default:
      return 23;
  }
}

// ---------------------------------------------------------------------------
// CRC16 (Modbus, init 0xFFFF) -- same function as lib/Hoymiles/src/crc.cpp
// ---------------------------------------------------------------------------
inline uint16_t fw_crc16(const uint8_t *buf, size_t len, uint16_t start = 0xFFFF) {
  uint16_t crc = start;
  for (size_t i = 0; i < len; i++) {
    crc ^= buf[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      uint8_t shift = crc & 0x0001;
      crc >>= 1;
      if (shift) crc ^= 0xA001;
    }
  }
  return crc;
}

// ---------------------------------------------------------------------------
// Intel HEX
// ---------------------------------------------------------------------------
enum class HexResult : uint8_t { DATA, END_OF_FILE, SKIP, ERROR };

inline int fw_hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

/// Decode one Intel HEX line into `out`: [len][addr hi][addr lo][type][data...] WITHOUT the trailing
/// Intel HEX checksum byte (out_len = number of bytes written, i.e. the "row" that the protocol sends).
///
/// `strict` additionally verifies the Intel HEX checksum and the declared length. This matters: the DTU
/// computes a fresh CRC16 over whatever bytes it sends, so a byte corrupted in transit (or a damaged
/// file) would otherwise reach the inverter looking perfectly valid.
inline HexResult fw_decode_hex_line(const char *line, size_t n, uint8_t *out, size_t cap, size_t &out_len,
                                    bool strict) {
  out_len = 0;
  while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n' || line[n - 1] == ' ' || line[n - 1] == '\t')) n--;
  while (n > 0 && (line[0] == ' ' || line[0] == '\t')) {
    line++;
    n--;
  }
  if (n == 0 || line[0] != ':') return HexResult::SKIP;
  if (n < 11 || ((n - 1) % 2) != 0) return HexResult::ERROR;  // at least len+addr(2)+type+checksum

  const size_t total = (n - 1) / 2;
  if (total > cap) return HexResult::ERROR;

  uint8_t sum = 0;
  for (size_t i = 0; i < total; i++) {
    int hi = fw_hex_nibble(line[1 + 2 * i]);
    int lo = fw_hex_nibble(line[2 + 2 * i]);
    if (hi < 0 || lo < 0) return HexResult::ERROR;
    out[i] = static_cast<uint8_t>((hi << 4) | lo);
    sum = static_cast<uint8_t>(sum + out[i]);
  }
  if (strict) {
    if (sum != 0) return HexResult::ERROR;                    // Intel HEX checksum
    if (out[0] != total - 5) return HexResult::ERROR;         // declared length vs. actual
  }
  out_len = total - 1;
  return out[3] == 0x01 ? HexResult::END_OF_FILE : HexResult::DATA;
}

// ---------------------------------------------------------------------------
// Which inverter a file is for (src/WebApi_devinfo.cpp: kFirmwareSerialRules)
// ---------------------------------------------------------------------------
struct FwSerialRule {
  uint16_t pre_serial;  // first 4 hex digits of the inverter serial number
  uint8_t new_gen1;     // identity byte [4], low nibble
  uint8_t phase;        // identity byte [5], high nibble
  uint8_t input_type;   // identity byte [5], low nibble
  uint8_t dsp;          // identity byte [6], high nibble
  uint8_t new_gen2;     // identity byte [6], low nibble
  uint8_t new_gen3;     // identity byte [7], high nibble
  uint8_t b_type;       // identity byte [7], low nibble
};

static const FwSerialRule FW_SERIAL_RULES[] = {
    // pre, gen1, phase, in, dsp, gen2, gen3, btype
    {0x1121, 0, 1, 0, 0, 0, 0, 0},  // HM 1T MI
    {0x1124, 0, 1, 0, 0, 0, 0, 0},  // HMS 1T MI
    {0x1125, 0, 1, 0, 0, 0, 0, 1},  // HMS 1T B
    {0x1126, 0, 1, 0, 0, 0, 0, 2},  // HMS 1T US
    {0x1400, 0, 1, 0, 0, 0, 0, 1},  // HMS 1T B
    {0x1141, 0, 1, 1, 0, 0, 0, 0},  // HM 2T MI
    {0x1143, 0, 1, 1, 1, 0, 0, 1},  // HMS 2T B
    {0x1144, 0, 1, 1, 1, 0, 0, 1},  // HMS 2T B
    {0x1146, 0, 1, 1, 1, 0, 0, 2},  // HMS 2T US
    {0x1410, 0, 1, 1, 1, 0, 0, 1},  // HMS 2T B
    {0x1161, 0, 1, 2, 0, 0, 0, 0},  // HM 4T MI
    {0x1162, 0, 4, 2, 0, 0, 0, 0},  // HME1 4T MI
    {0x1164, 0, 1, 2, 1, 0, 0, 0},  // HMS 4T MI
    {0x1165, 0, 1, 2, 2, 0, 0, 0},  // HMS 4T MI (2000B_T)
    {0x1166, 0, 1, 2, 1, 1, 0, 1},  // HMS 4T B (2000C_B)
    {0x1421, 0, 1, 2, 1, 1, 0, 1},  // HMS 4T B (2000C_B)
    {0x1620, 0, 1, 2, 3, 0, 0, 1},  // HMS 4T B (WB_B)
    {0x1361, 0, 3, 5, 0, 0, 0, 0},  // HMT 4T MI
    {0x1362, 0, 3, 6, 0, 0, 0, 0},  // HMT 4T MI (NA R)
    {0x1382, 0, 3, 3, 0, 0, 0, 0},  // HMT 6T MI
    {0x1520, 0, 1, 6, 0, 0, 0, 0},  // MIT-5000 MI
};

/// The reference implementation additionally keeps an allow-list of serial prefixes; it is the same set
/// as the rules above except for 0x1520 (MIT-5000), which has a rule but is NOT allowed.
inline const FwSerialRule *fw_find_rule(uint64_t serial) {
  const uint16_t pre = static_cast<uint16_t>((serial >> 32) & 0xFFFF);
  if (pre == 0x1520) return nullptr;
  for (const auto &r : FW_SERIAL_RULES) {
    if (r.pre_serial == pre) return &r;
  }
  return nullptr;
}

/// Check the identity bytes (row[4..7], i.e. the first four data bytes of the first HEX row) against the
/// inverter's rule. `row` is a decoded row (see fw_decode_hex_line), `row_len` >= 8.
inline bool fw_identity_matches(const FwSerialRule &r, const uint8_t *row, size_t row_len) {
  if (row_len < 8) return false;
  if ((row[4] & 0x0F) != r.new_gen1) return false;
  if ((row[5] >> 4) != r.phase || (row[5] & 0x0F) != r.input_type) return false;
  if ((row[6] >> 4) != r.dsp || (row[6] & 0x0F) != r.new_gen2) return false;
  if ((row[7] >> 4) != r.new_gen3 || (row[7] & 0x0F) != r.b_type) return false;
  return true;
}

// ---------------------------------------------------------------------------
// Row -> radio packets (HM_Abstract::enqueueFirmwareRow)
// ---------------------------------------------------------------------------
struct FwPacket {
  uint8_t number;                // byte 9 of the frame; FW_ACK_FLAG set = last packet of the row
  uint8_t data[FW_CHUNK_SIZE];   // payload slice
  uint8_t data_len;
  bool has_crc;                  // the row CRC16 (big endian) follows the payload
};

/// Split a row into 16-byte packets. The row CRC16 rides in the last packet when it fits (<= 14 payload
/// bytes), otherwise in an extra packet that carries only the CRC. Returns the number of packets (0 = error).
inline size_t fw_packetize_row(const uint8_t *row, size_t row_len, FwPacket *out, size_t max_out) {
  if (row_len == 0 || row_len > FW_MAX_ROW_LEN) return 0;
  size_t count = 0;
  uint8_t packet_no = 1;
  for (size_t off = 0; off < row_len; off += FW_CHUNK_SIZE) {
    const size_t chunk = (row_len - off < FW_CHUNK_SIZE) ? (row_len - off) : FW_CHUNK_SIZE;
    const bool last = (off + chunk >= row_len);
    const bool crc_fits = last && chunk <= static_cast<size_t>(FW_CHUNK_SIZE - FW_CRC_SIZE);
    if (count >= max_out) return 0;
    FwPacket &p = out[count++];
    p.number = crc_fits ? static_cast<uint8_t>(FW_ACK_FLAG + packet_no) : packet_no;
    memset(p.data, 0, sizeof(p.data));
    memcpy(p.data, row + off, chunk);
    p.data_len = static_cast<uint8_t>(chunk);
    p.has_crc = crc_fits;
    packet_no++;
    if (last && !crc_fits) {
      if (count >= max_out) return 0;
      FwPacket &c = out[count++];
      c.number = static_cast<uint8_t>(FW_ACK_FLAG + packet_no);
      memset(c.data, 0, sizeof(c.data));
      c.data_len = 0;
      c.has_crc = true;
      packet_no++;
    }
  }
  return count;
}

inline bool fw_packet_expects_ack(const FwPacket &p) { return (p.number & FW_ACK_FLAG) != 0; }

/// Build the radio frame of one packet (FirmwareCommand::getDataPayload):
///   [0]=0x0E  [1..4]=inverter id  [5..8]=DTU id  [9]=packet number  [10..]=payload  [+CRC16 BE]  [XOR of all]
/// NOTE: the trailing byte is a plain XOR of all previous bytes, NOT the CRC8 of the other Hoymiles commands.
/// `out` must hold at least 32 bytes. Returns the frame length.
inline uint8_t fw_build_frame(const uint8_t inv_id[4], const uint8_t dtu_id[4], const FwPacket &p, const uint8_t *row,
                              size_t row_len, uint8_t *out) {
  out[0] = FW_CMD_DOWN_PRO;
  memcpy(&out[1], inv_id, 4);
  memcpy(&out[5], dtu_id, 4);
  out[9] = p.number;
  uint8_t n = 10;
  memcpy(&out[n], p.data, p.data_len);
  n = static_cast<uint8_t>(n + p.data_len);
  if (p.has_crc) {
    const uint16_t crc = fw_crc16(row, row_len, 0xFFFF);
    out[n++] = static_cast<uint8_t>(crc >> 8);
    out[n++] = static_cast<uint8_t>(crc & 0xFF);
  }
  uint8_t x = 0;
  for (uint8_t i = 0; i < n; i++) x ^= out[i];
  out[n++] = x;
  return n;
}

/// Is `frame` (a received, CRC8-checked frame) the acknowledgement of `row`? The inverter answers with
/// command 0x8E and echoes the first four bytes of the row (len, address, type) as payload.
inline bool fw_is_row_ack(const uint8_t *frame, uint8_t len, const uint8_t *row, size_t row_len) {
  if (len < 11) return false;
  if (frame[0] != static_cast<uint8_t>(FW_CMD_DOWN_PRO | 0x80)) return false;
  const size_t need = row_len < 4 ? row_len : 4;
  const size_t plen = static_cast<size_t>(len) - 11;  // 10-byte header + 1-byte CRC8
  return plen >= need && memcmp(&frame[10], row, need) == 0;
}

/// "0E 71 60 ..." -- same layout as the reference implementation's TX debug log.
inline void fw_hex_dump(const uint8_t *buf, size_t len, char *out, size_t cap) {
  size_t pos = 0;
  for (size_t i = 0; i < len && pos + 4 < cap; i++) {
    pos += static_cast<size_t>(snprintf(out + pos, cap - pos, i ? " %02X" : "%02X", buf[i]));
  }
  if (cap) out[pos < cap ? pos : cap - 1] = '\0';
}

}  // namespace hms_fwupdate
}  // namespace esphome
