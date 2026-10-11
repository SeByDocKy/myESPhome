#include "aps_protocol.h"

namespace esphome {
namespace apszb {

// ---------------------------------------------------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------------------------------------------------
static inline void put16(uint8_t *p, uint16_t v) {
  p[0] = uint8_t(v & 0xFF);
  p[1] = uint8_t(v >> 8);
}
static inline uint16_t get16(const uint8_t *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
static inline uint16_t be16(const uint8_t *p) { return (uint16_t(p[0]) << 8) | p[1]; }
static inline uint32_t be24(const uint8_t *p) { return (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2]; }
static inline uint32_t be32(const uint8_t *p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

bool parse_hex_bytes(const char *hex, uint8_t *out, size_t n) {
  if (hex == nullptr || strlen(hex) != n * 2)
    return false;
  for (size_t i = 0; i < n; i++) {
    int hi = hex_nibble(hex[2 * i]);
    int lo = hex_nibble(hex[2 * i + 1]);
    if (hi < 0 || lo < 0)
      return false;
    out[i] = uint8_t((hi << 4) | lo);
  }
  return true;
}

// appends a constant given as hex text
static size_t put_hex(uint8_t *out, const char *hex) {
  size_t n = strlen(hex) / 2;
  parse_hex_bytes(hex, out, n);
  return n;
}

// ---------------------------------------------------------------------------------------------------------------------
// Frame builders
// ---------------------------------------------------------------------------------------------------------------------
size_t FrameFactory::put_mac_header_(uint8_t *p, bool ack_request, uint16_t dst_pan, uint16_t dst, uint16_t src) {
  // IEEE 802.15.4 data frame, PAN id compression, short destination, short source
  uint16_t fcf = 0x0001 | 0x0040 | (0x2 << 10) | (0x2 << 14);
  if (ack_request)
    fcf |= 0x0020;
  put16(p, fcf);
  p[2] = this->mac_seq_++;
  put16(p + 3, dst_pan);
  put16(p + 5, dst);
  put16(p + 7, src);
  return 9;
}

size_t FrameFactory::put_nwk_header_(uint8_t *p, uint8_t frame_type, bool discover, uint16_t dst, uint16_t src,
                                     uint8_t radius) {
  uint16_t fc = uint16_t(frame_type & 0x03) | (uint16_t(this->cfg_->nwk_protocol_version & 0x0F) << 2);
  if (discover)
    fc |= (1 << 6);
  put16(p, fc);
  put16(p + 2, dst);
  put16(p + 4, src);
  p[6] = radius;
  p[7] = this->nwk_seq_++;
  return 8;
}

bool FrameFactory::build_unicast(TxFrame &f, uint16_t dst, uint16_t cluster, const uint8_t *asdu, size_t n,
                                 bool aps_ack) {
  if (9 + 8 + 8 + n > MAX_PSDU - 2)
    return false;
  uint8_t *p = f.buf + 1;
  size_t i = 0;
  i += this->put_mac_header_(p + i, this->cfg_->mac_ack, this->cfg_->pan_id, dst, ADDR_COORDINATOR);
  i += this->put_nwk_header_(p + i, 0, this->cfg_->discover_route, dst, ADDR_COORDINATOR, this->cfg_->radius);
  p[i++] = aps_ack ? 0x40 : 0x00;  // APS data frame, unicast
  p[i++] = APS_ENDPOINT;           // destination endpoint
  put16(p + i, cluster);
  i += 2;
  put16(p + i, APS_PROFILE_ID);
  i += 2;
  p[i++] = APS_ENDPOINT;  // source endpoint
  p[i++] = this->aps_counter_++;
  memcpy(p + i, asdu, n);
  i += n;
  f.mac_len = i;
  f.ack_request = this->cfg_->mac_ack;
  f.msg.valid = n <= MAX_ASDU;
  f.msg.broadcast = false;
  f.msg.dst = dst;
  f.msg.pan = this->cfg_->pan_id;
  f.msg.cluster = cluster;
  f.msg.len = uint8_t(n <= MAX_ASDU ? n : 0);
  if (f.msg.valid)
    memcpy(f.msg.asdu, asdu, n);
  f.finish();
  return true;
}

bool FrameFactory::build_broadcast(TxFrame &f, uint16_t mac_pan, uint16_t cluster, const uint8_t *asdu, size_t n) {
  if (9 + 8 + 8 + n > MAX_PSDU - 2)
    return false;
  uint8_t *p = f.buf + 1;
  size_t i = 0;
  i += this->put_mac_header_(p + i, false, mac_pan, ADDR_BROADCAST, ADDR_COORDINATOR);
  i += this->put_nwk_header_(p + i, 0, false, ADDR_BROADCAST, ADDR_COORDINATOR, this->cfg_->radius);
  p[i++] = 0x08;  // APS data frame, broadcast delivery mode
  p[i++] = APS_ENDPOINT;
  put16(p + i, cluster);
  i += 2;
  put16(p + i, APS_PROFILE_ID);
  i += 2;
  p[i++] = APS_ENDPOINT;
  p[i++] = this->aps_counter_++;
  memcpy(p + i, asdu, n);
  i += n;
  f.mac_len = i;
  f.ack_request = false;
  f.msg.valid = n <= MAX_ASDU;
  f.msg.broadcast = true;
  f.msg.dst = ADDR_BROADCAST;
  f.msg.pan = mac_pan;
  f.msg.cluster = cluster;
  f.msg.len = uint8_t(n <= MAX_ASDU ? n : 0);
  if (f.msg.valid)
    memcpy(f.msg.asdu, asdu, n);
  f.finish();
  return true;
}

bool FrameFactory::build_link_status(TxFrame &f, const uint16_t *neighbours, size_t n) {
  if (n > 20)
    n = 20;
  uint8_t *p = f.buf + 1;
  size_t i = 0;
  i += this->put_mac_header_(p + i, false, this->cfg_->pan_id, ADDR_BROADCAST, ADDR_COORDINATOR);
  i += this->put_nwk_header_(p + i, 1, false, ADDR_ALL_ROUTERS, ADDR_COORDINATOR, 1);
  p[i++] = 0x08;                  // NWK command: link status
  p[i++] = uint8_t(0x60 | (n & 0x1F));  // first frame + last frame + entry count
  for (size_t k = 0; k < n; k++) {
    put16(p + i, neighbours[k]);
    i += 2;
    p[i++] = 0x11;  // incoming cost 1, outgoing cost 1
  }
  f.mac_len = i;
  f.ack_request = false;
  f.finish();
  return true;
}

bool FrameFactory::build_many_to_one_rreq(TxFrame &f) {
  uint8_t *p = f.buf + 1;
  size_t i = 0;
  i += this->put_mac_header_(p + i, false, this->cfg_->pan_id, ADDR_BROADCAST, ADDR_COORDINATOR);
  i += this->put_nwk_header_(p + i, 1, false, ADDR_ALL_ROUTERS, ADDR_COORDINATOR, 30);
  p[i++] = 0x01;  // NWK command: route request
  p[i++] = 0x08;  // many-to-one, with route record table
  p[i++] = this->rreq_id_++;
  put16(p + i, ADDR_ALL_ROUTERS);  // destination address
  i += 2;
  p[i++] = 0x00;  // path cost
  f.mac_len = i;
  f.ack_request = false;
  f.finish();
  return true;
}

bool FrameFactory::build_aps_ack(TxFrame &f, const RxFrame &rx) {
  uint8_t *p = f.buf + 1;
  size_t i = 0;
  i += this->put_mac_header_(p + i, true, this->cfg_->pan_id, rx.mac_src, ADDR_COORDINATOR);
  i += this->put_nwk_header_(p + i, 0, false, rx.nwk_src, ADDR_COORDINATOR, this->cfg_->radius);
  p[i++] = uint8_t(0x02 | (rx.aps_ext ? 0x80 : 0x00));  // APS acknowledgement (data frame ack format)
  p[i++] = rx.src_ep;
  put16(p + i, rx.cluster);
  i += 2;
  put16(p + i, rx.profile);
  i += 2;
  p[i++] = rx.dst_ep;
  p[i++] = rx.aps_counter;
  if (rx.aps_ext) {
    p[i++] = rx.frag & 0x03;  // extended frame control: fragmentation field
    p[i++] = rx.block;        // block number
    p[i++] = 0x01;            // ack bitfield: block received
  }
  f.mac_len = i;
  f.ack_request = true;
  f.finish();
  return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Frame parser
// ---------------------------------------------------------------------------------------------------------------------
bool parse_frame(const uint8_t *p, size_t len, RxFrame &o) {
  o = RxFrame();
  if (p == nullptr || len < 5)
    return false;
  uint16_t fcf = get16(p);
  o.mac_type = fcf & 0x07;
  bool mac_sec = (fcf & 0x08) != 0;
  o.mac_ack_request = (fcf & 0x20) != 0;
  bool pan_comp = (fcf & 0x40) != 0;
  uint8_t dst_mode = (fcf >> 10) & 0x03;
  uint8_t src_mode = (fcf >> 14) & 0x03;
  size_t i = 2;
  o.mac_seq = p[i++];

  if (dst_mode == 2 || dst_mode == 3) {
    size_t need = 2 + (dst_mode == 2 ? 2 : 8);
    if (i + need > len)
      return false;
    o.mac_pan = get16(p + i);
    i += 2;
    if (dst_mode == 2) {
      o.mac_dst = get16(p + i);
      i += 2;
    } else {
      o.mac_dst = 0xFFFE;
      i += 8;
    }
  }
  if (src_mode == 2 || src_mode == 3) {
    size_t need = (pan_comp ? 0 : 2) + (src_mode == 2 ? 2 : 8);
    if (i + need > len)
      return false;
    if (!pan_comp) {
      if (dst_mode == 0)
        o.mac_pan = get16(p + i);
      i += 2;
    }
    if (src_mode == 2) {
      o.mac_src = get16(p + i);
      i += 2;
    } else {
      o.mac_src = 0xFFFE;
      i += 8;
    }
  }
  o.mac_ok = true;
  if (o.mac_type != 1 || mac_sec)
    return true;

  // ---- NWK ----
  if (i + 8 > len)
    return true;
  uint16_t nfc = get16(p + i);
  o.nwk_type = nfc & 0x03;
  o.nwk_version = (nfc >> 2) & 0x0F;
  o.nwk_secured = (nfc & 0x0200) != 0;
  bool mcast = (nfc & 0x0100) != 0;
  bool src_route = (nfc & 0x0400) != 0;
  bool dst_ieee = (nfc & 0x0800) != 0;
  bool src_ieee = (nfc & 0x1000) != 0;
  o.nwk_dst = get16(p + i + 2);
  o.nwk_src = get16(p + i + 4);
  o.nwk_radius = p[i + 6];
  i += 8;
  if (o.nwk_secured) {
    o.nwk_ok = true;  // cannot go any further
    return true;
  }
  if (mcast)
    i += 1;
  if (dst_ieee)
    i += 8;
  if (src_ieee)
    i += 8;
  if (src_route) {
    if (i + 2 > len)
      return true;
    uint8_t relay_count = p[i];
    i += 2 + 2 * size_t(relay_count);
  }
  if (i > len)
    return true;
  o.nwk_ok = true;
  if (o.nwk_type == 1) {
    if (i < len)
      o.nwk_cmd = p[i];
    return true;
  }
  if (o.nwk_type != 0 || i >= len)
    return true;

  // ---- APS ----
  uint8_t afc = p[i++];
  o.aps_type = afc & 0x03;
  o.aps_delivery = (afc >> 2) & 0x03;
  bool ack_format = (afc & 0x10) != 0;
  o.aps_secured = (afc & 0x20) != 0;
  o.aps_ack_request = (afc & 0x40) != 0;
  o.aps_ext = (afc & 0x80) != 0;
  if (o.aps_secured) {
    o.aps_ok = true;
    return true;
  }
  if (o.aps_type == 0 || (o.aps_type == 2 && !ack_format)) {
    size_t need = (o.aps_delivery == 3 ? 2 : 1) + 2 + 2 + 1 + 1;
    if (o.aps_type == 2 && o.aps_delivery == 3)
      need -= 1;
    if (i + need > len)
      return true;
    if (o.aps_delivery == 3) {
      i += 2;  // group address
    } else {
      o.dst_ep = p[i++];
    }
    o.cluster = get16(p + i);
    i += 2;
    o.profile = get16(p + i);
    i += 2;
    o.src_ep = p[i++];
    o.aps_counter = p[i++];
    if (o.aps_ext && o.aps_type == 0) {
      if (i >= len)
        return true;
      uint8_t efc = p[i++];
      o.frag = efc & 0x03;
      if (o.frag != 0) {
        if (i >= len)
          return true;
        o.block = p[i++];
      }
    }
  }
  o.aps_ok = true;
  if (o.aps_type == 0 && i <= len) {
    o.payload = p + i;
    o.payload_len = len - i;
  }
  return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Reassembly
// ---------------------------------------------------------------------------------------------------------------------
bool Reassembler::feed(const RxFrame &rx, uint32_t now_ms, const uint8_t **out, size_t *out_len) {
  if (rx.payload == nullptr)
    return false;
  if (rx.frag == 0) {
    *out = rx.payload;
    *out_len = rx.payload_len;
    return true;
  }
  if (rx.frag == 1) {
    if (rx.block < 2 || rx.block > 8 || rx.payload_len == 0 || rx.payload_len > REASSEMBLY_MAX)
      return false;
    this->total_ = rx.block;
    this->mask_ = 0x01;
    this->first_len_ = rx.payload_len;
    this->last_len_ = 0;
    this->src_ = rx.nwk_src;
    this->cluster_ = rx.cluster;
    this->started_ = now_ms;
    memcpy(this->buf_, rx.payload, rx.payload_len);
  } else {
    if (this->total_ == 0 || rx.nwk_src != this->src_ || rx.cluster != this->cluster_ ||
        now_ms - this->started_ > 5000 || rx.block >= this->total_ || rx.block == 0) {
      this->reset();
      return false;
    }
    size_t offset = size_t(rx.block) * this->first_len_;
    if (offset + rx.payload_len > REASSEMBLY_MAX) {
      this->reset();
      return false;
    }
    memcpy(this->buf_ + offset, rx.payload, rx.payload_len);
    this->mask_ |= uint8_t(1u << rx.block);
    if (rx.block == this->total_ - 1)
      this->last_len_ = rx.payload_len;
  }
  uint8_t full = uint8_t((1u << this->total_) - 1u);
  if (this->mask_ == full && this->last_len_ != 0) {
    *out = this->buf_;
    *out_len = size_t(this->total_ - 1) * this->first_len_ + this->last_len_;
    this->mask_ = 0;
    this->total_ = 0;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Application payloads
// ---------------------------------------------------------------------------------------------------------------------
size_t asdu_poll(uint8_t *out, const uint8_t ecu_rev[6]) {
  memcpy(out, ecu_rev, 6);
  return 6 + put_hex(out + 6, "FBFB06BB000000000000C1FEFE");
}

size_t asdu_query(uint8_t *out, const uint8_t ecu_rev[6]) {
  memcpy(out, ecu_rev, 6);
  return 6 + put_hex(out + 6, "FBFB06DE000000000000E4FEFE");
}

size_t asdu_query_flush(uint8_t *out, const uint8_t ecu_rev[6]) {
  memcpy(out, ecu_rev, 6);
  return 6 + put_hex(out + 6, "FBFB06DE00000000000000FEFE");
}

size_t asdu_reboot(uint8_t *out, const uint8_t ecu_rev[6]) {
  memcpy(out, ecu_rev, 6);
  return 6 + put_hex(out + 6, "FBFB06C1000000000000A6FEFE");
}

size_t asdu_normal_ops(uint8_t *out, const uint8_t ecu_rev[6]) {
  memcpy(out, ecu_rev, 6);
  return 6 + put_hex(out + 6, "FBFB1100000D6030FBD3000000000000000004010281FEFE");
}

size_t asdu_throttle(uint8_t *out, const uint8_t ecu_rev[6], InverterType type, int watts) {
  if (watts < 0)
    watts = 0;
  memcpy(out, ecu_rev, 6);
  size_t n = 6;
  out[n++] = 0xFB;
  out[n++] = 0xFB;
  if (type == InverterType::DS3) {
    int scaled = int(watts * 16.59);
    uint8_t msb = uint8_t((scaled >> 8) & 0xFF);
    uint8_t lsb = uint8_t(scaled & 0xFF);
    uint8_t s = uint8_t((msb + lsb) & 0xFF);
    uint8_t vv = uint8_t((s - 0x29) & 0xFF);
    const uint8_t head[] = {0x06, 0xAA, 0x27, 0x00, 0x00};
    memcpy(out + n, head, sizeof(head));
    n += sizeof(head);
    out[n++] = msb;
    out[n++] = lsb;
    out[n++] = 0x01;
    out[n++] = vv;
  } else {
    int scaled = int(watts * 28.89);
    uint8_t hi = uint8_t((scaled >> 8) & 0xFF);
    uint8_t lo = uint8_t(scaled & 0xFF);
    const uint8_t head[] = {0x06, 0x1C, 0x8C, 0x02};
    memcpy(out + n, head, sizeof(head));
    n += sizeof(head);
    out[n++] = hi;
    out[n++] = lo;
    out[n++] = 0x00;
    uint16_t sum = 0x06 + 0x1C + 0x8C + 0x02 + hi + lo + 0x00;
    out[n++] = uint8_t(sum >> 8);
    out[n++] = uint8_t(sum & 0xFF);
  }
  out[n++] = 0xFE;
  out[n++] = 0xFE;
  return n;
}

size_t asdu_pair(uint8_t *out, int step, const NetworkConfig &cfg, const uint8_t serial[6], uint16_t *cluster) {
  uint8_t rev[6];
  cfg.ecu_reverse(rev);
  size_t n = 0;
  switch (step) {
    case 0:
      *cluster = CLUSTER_PAIR_0;
      memcpy(out, serial, 6);
      n = 6;
      out[n++] = 0xFF;
      out[n++] = 0xFF;
      out[n++] = cfg.channel;
      out[n++] = 0xFF;
      out[n++] = 0xFF;
      memcpy(out + n, rev, 6);
      n += 6;
      break;
    case 1:
      *cluster = CLUSTER_PAIR_1;
      memcpy(out, serial, 6);
      n = 6;
      break;
    case 2:
      *cluster = CLUSTER_PAIR_2;
      memcpy(out, serial, 6);
      n = 6;
      out[n++] = cfg.ecu_id[1];
      out[n++] = cfg.ecu_id[0];
      out[n++] = cfg.channel;
      out[n++] = 0xFF;
      out[n++] = 0xFF;
      memcpy(out + n, rev, 6);
      n += 6;
      break;
    default:
      *cluster = CLUSTER_PAIR_3;
      memcpy(out, rev, 6);
      n = 6;
      break;
  }
  return n;
}

// ---------------------------------------------------------------------------------------------------------------------
// Decoders
// ---------------------------------------------------------------------------------------------------------------------
bool asdu_has_serial(const uint8_t *asdu, size_t len, const uint8_t serial[6]) {
  return len >= 6 && memcmp(asdu, serial, 6) == 0;
}

bool decode_poll(const uint8_t *a, size_t len, InverterType type, PollData &o) {
  o = PollData();
  if (a == nullptr || len < 8 || a[6] != 0xFB || a[7] != 0xFB)
    return false;

  if (type == InverterType::DS3) {
    o.channels = 2;
    if (len < 58)
      return false;
    o.ac_voltage = be16(a + 34) / 3.8f;
    o.frequency = be16(a + 36) / 100.0f;
    o.temperature = be16(a + 48) * 0.0198f - 23.84f;
    o.dc_voltage[0] = be16(a + 26) / 48.0f;
    o.dc_voltage[1] = be16(a + 28) / 48.0f;
    o.dc_current[0] = be16(a + 30) * 0.0125f;
    o.dc_current[1] = be16(a + 32) * 0.0125f;
    o.timestamp = be16(a + 38);
    for (int x = 0; x < 2; x++)
      o.energy_wh[x] = (be32(a + 50 + 4 * x) / 1000.0f / 100.0f) * 1.66f;
    return true;
  }

  // YC600 / QS1
  const bool qs1 = (type == InverterType::QS1);
  o.channels = qs1 ? 4 : 2;
  const size_t need = 37 + size_t(o.channels - 1) * 5 + 3;
  if (len < need || (qs1 && len < 32))
    return false;
  o.ac_voltage = be16(a + 28) * (1.0f / 1.3277f) / 4.0f;
  uint32_t period = be24(a + 12);
  o.frequency = period ? 50000000.0f / float(period) : 0.0f;
  o.temperature = be16(a + 10) * 0.2752f - 258.7f;

  auto volt = [](uint8_t hi8, uint8_t nib_byte) -> float {
    return float((unsigned(hi8) << 4) | (nib_byte >> 4)) * 82.5f / 4096.0f;
  };
  auto amp = [](uint8_t nib_byte, uint8_t lo8) -> float {
    return float(((unsigned(nib_byte) & 0x0F) << 8) | lo8) * 27.5f / 4096.0f;
  };
  o.dc_voltage[1] = volt(a[24], a[23]);
  o.dc_current[1] = amp(a[23], a[22]);
  o.dc_voltage[0] = volt(a[27], a[26]);
  o.dc_current[0] = amp(a[26], a[25]);
  if (qs1) {
    o.dc_voltage[2] = volt(a[21], a[20]);
    o.dc_current[2] = amp(a[20], a[19]);
    o.dc_voltage[3] = volt(a[18], a[17]);
    o.dc_current[3] = amp(a[17], a[16]);
    o.timestamp = be16(a + 30);
  } else {
    o.timestamp = be16(a + 17);
  }
  for (int x = 0; x < o.channels; x++)
    o.energy_wh[x] = float(be24(a + 37 + 5 * x)) * 8.311f / 3600.0f;
  return true;
}

bool decode_max_power(const uint8_t *a, size_t len, InverterType type, float *watts) {
  if (a == nullptr || len < 15 || a[6] != 0xFB || a[7] != 0xFB)
    return false;
  if (type == InverterType::DS3) {
    *watts = be16(a + 13) / 16.59f;
    return true;
  }
  // YC600 / QS1: the programmed value sits right before the marker 3B 66
  for (size_t i = 10; i + 1 < len; i++) {
    if (a[i] == 0x3B && a[i + 1] == 0x66) {
      *watts = be16(a + i - 2) / 28.89f;
      return true;
    }
  }
  return false;
}

}  // namespace apszb
}  // namespace esphome
