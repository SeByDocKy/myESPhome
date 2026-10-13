#include "znp_radio.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace esphome {
namespace apszb {

// ZNP command identifiers
static const uint8_t SYS_RESET_REQ_0 = 0x41, SYS_RESET_REQ_1 = 0x00;
static const uint8_t SYS_RESET_IND = 0x80;                  // cmd0 0x41 (AREQ SYS)
static const uint8_t AF_DATA_REQUEST = 0x01;                // cmd0 0x24 (SREQ AF)
static const uint8_t AF_DATA_REQUEST_EXT = 0x02;
static const uint8_t ZDO_STATE_CHANGE_IND = 0xC0;           // cmd0 0x45 (AREQ ZDO)
static const uint8_t DEV_ZB_COORD = 0x09;
static const uint8_t AF_DATA_CONFIRM = 0x80;                // cmd0 0x44 (AREQ AF)
static const uint8_t AF_INCOMING_MSG = 0x81;
static const uint8_t TYPE_SREQ_AF = 0x24, TYPE_SRSP_AF = 0x64, TYPE_AREQ_AF = 0x44, TYPE_AREQ_ZDO = 0x45,
                     TYPE_AREQ_SYS = 0x41;

static const uint32_t RESET_PULSE_MS = 50;
static const uint32_t RESET_SETTLE_MS = 500;
static const uint32_t START_TIMEOUT_MS = 12000;
static const uint32_t RETRY_DELAY_MS = 15000;
static const uint8_t DEFAULT_RADIUS = 0x0F;

// ---------------------------------------------------------------------------------------------------------------------
// framing
// ---------------------------------------------------------------------------------------------------------------------
size_t znp_encode(uint8_t cmd0, uint8_t cmd1, const uint8_t *data, size_t len, uint8_t *out) {
  if (len > ZNP_MAX_DATA)
    len = ZNP_MAX_DATA;
  size_t i = 0;
  out[i++] = 0xFE;
  out[i++] = uint8_t(len);
  out[i++] = cmd0;
  out[i++] = cmd1;
  uint8_t fcs = uint8_t(len) ^ cmd0 ^ cmd1;
  for (size_t k = 0; k < len; k++) {
    out[i++] = data[k];
    fcs ^= data[k];
  }
  out[i++] = fcs;
  return i;
}

bool ZnpParser::feed(uint8_t b, uint32_t now_ms, ZnpFrame &out) {
  if (this->state_ != SOF && int32_t(now_ms - this->last_byte_) > 250) {
    this->state_ = SOF;  // truncated frame
    this->bad_++;
  }
  this->last_byte_ = now_ms;
  switch (this->state_) {
    case SOF:
      if (b == 0xFE)
        this->state_ = LEN;
      break;
    case LEN:
      this->cur_.len = b;
      this->fcs_ = b;
      this->state_ = CMD0;
      break;
    case CMD0:
      this->cur_.cmd0 = b;
      this->fcs_ ^= b;
      this->state_ = CMD1;
      break;
    case CMD1:
      this->cur_.cmd1 = b;
      this->fcs_ ^= b;
      this->pos_ = 0;
      this->state_ = this->cur_.len == 0 ? FCS : DATA;
      break;
    case DATA:
      this->cur_.data[this->pos_++] = b;
      this->fcs_ ^= b;
      if (this->pos_ >= this->cur_.len)
        this->state_ = FCS;
      break;
    case FCS:
      this->state_ = SOF;
      if (b == this->fcs_) {
        out = this->cur_;
        return true;
      }
      this->bad_++;
      break;
  }
  return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// radio
// ---------------------------------------------------------------------------------------------------------------------
void ZnpRadio::log_(int level, const char *fmt, ...) {
  if (this->log_fn_ == nullptr)
    return;
  char buf[200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  this->log_fn_(level, buf);
}

void ZnpRadio::begin(const NetworkConfig &cfg) {
  this->cfg_ = cfg;
  // The configuration sequence of ZIGBEE_COORDINATOR.ino of the original project (CC2530 as ZigBee coordinator)
  this->init_n_ = 0;
  auto add = [this](uint8_t c0, uint8_t c1, std::initializer_list<uint8_t> d, const char *name, uint16_t timeout) {
    InitCmd &c = this->init_[this->init_n_++];
    c.cmd0 = c0;
    c.cmd1 = c1;
    c.len = uint8_t(d.size());
    size_t k = 0;
    for (uint8_t v : d)
      c.data[k++] = v;
    c.name = name;
    c.timeout_ms = timeout;
  };
  uint8_t ext[8];
  cfg.ieee_address(ext);
  // ZB_WRITE_CONFIGURATION (0x26 0x05): ConfigId, Len, Value
  add(0x26, 0x05, {0x03, 0x01, 0x03}, "startup option: clear state", 1000);
  add(TYPE_AREQ_SYS, SYS_RESET_REQ_1, {0x00}, "SYS_RESET_REQ", 3000);
  add(0x26, 0x05, {0x01, 0x08, ext[0], ext[1], ext[2], ext[3], ext[4], ext[5], ext[6], ext[7]}, "extended address", 1000);
  add(0x26, 0x05, {0x87, 0x01, 0x00}, "logical type: coordinator", 1000);
  add(0x26, 0x05, {0x83, 0x02, uint8_t(cfg.pan_id & 0xFF), uint8_t(cfg.pan_id >> 8)}, "PAN id", 1000);
  const uint32_t chan = 1UL << cfg.channel;
  add(0x26, 0x05, {0x84, 0x04, uint8_t(chan), uint8_t(chan >> 8), uint8_t(chan >> 16), uint8_t(chan >> 24)},
      "channel list", 1000);
  // AF_REGISTER: endpoint 0x14, profile 0x0F05, device 0x0100, version 1, 2 input clusters (0x0000, 0x0015), 0 output
  add(0x24, 0x00, {0x14, 0x05, 0x0F, 0x00, 0x01, 0x01, 0x00, 0x02, 0x00, 0x00, 0x15, 0x00, 0x00}, "AF_REGISTER", 1000);
  this->state_ = IDLE;  // the first loop() starts the sequence
  this->retry_at_ = 0;
}

void ZnpRadio::restart_(uint32_t now) {
  this->init_count_++;
  this->init_idx_ = 0;
  this->first_cmd_answered_ = false;
  this->tx_state_ = 0;
  this->state_since_ = now;
  if (this->port_ != nullptr && this->port_->has_reset()) {
    this->port_->set_reset(true);
    this->state_ = RESET_ASSERT;
  } else {
    this->state_ = RESET_WAIT;
  }
}

void ZnpRadio::send_(uint8_t cmd0, uint8_t cmd1, const uint8_t *data, size_t len) {
  uint8_t buf[ZNP_MAX_DATA + 5];
  size_t n = znp_encode(cmd0, cmd1, data, len, buf);
  this->port_->write(buf, n);
}

const char *ZnpRadio::tx_error_name(int code) {
  switch (code) {
    case 0:
      return "ok";
    case 98:
      return "modem not answering";
    case 99:
      return "modem not ready";
    case 1 + 0x01:
      return "invalid parameter";
    case 1 + 0x10:
      return "memory error";
    default:
      return "modem error";
  }
}

bool ZnpRadio::transmit(const TxFrame &f) {
  if (this->state_ != READY || this->port_ == nullptr || this->tx_state_ == -1)
    return false;
  if (!f.msg.valid)
    return false;
  uint8_t buf[ZNP_MAX_DATA];
  size_t i = 0;
  const uint8_t tid = this->trans_id_++;
  if (f.msg.broadcast && f.msg.pan == ADDR_BROADCAST) {
    // pairing: AF_DATA_REQUEST_EXT, broadcast address mode (0x0F), destination PAN 0xFFFF
    buf[i++] = 0x0F;
    memset(buf + i, 0xFF, 8);
    i += 8;
    buf[i++] = APS_ENDPOINT;
    buf[i++] = 0xFF;  // destination PAN
    buf[i++] = 0xFF;
    buf[i++] = APS_ENDPOINT;
    buf[i++] = uint8_t(f.msg.cluster & 0xFF);
    buf[i++] = uint8_t(f.msg.cluster >> 8);
    buf[i++] = tid;
    buf[i++] = 0x00;  // options
    buf[i++] = DEFAULT_RADIUS;
    buf[i++] = f.msg.len;  // 16 bit length
    buf[i++] = 0x00;
  } else {
    // AF_DATA_REQUEST: DstAddr(2) DstEP SrcEP Cluster(2) TransId Options Radius Len
    buf[i++] = uint8_t(f.msg.dst & 0xFF);
    buf[i++] = uint8_t(f.msg.dst >> 8);
    buf[i++] = APS_ENDPOINT;
    buf[i++] = APS_ENDPOINT;
    buf[i++] = uint8_t(f.msg.cluster & 0xFF);
    buf[i++] = uint8_t(f.msg.cluster >> 8);
    buf[i++] = tid;
    buf[i++] = 0x00;
    buf[i++] = DEFAULT_RADIUS;
    buf[i++] = f.msg.len;
  }
  memcpy(buf + i, f.msg.asdu, f.msg.len);
  i += f.msg.len;
  const bool ext = f.msg.broadcast && f.msg.pan == ADDR_BROADCAST;
  this->tx_state_ = -1;
  this->send_(TYPE_SREQ_AF, ext ? AF_DATA_REQUEST_EXT : AF_DATA_REQUEST, buf, i);
  if (this->log_frames_)
    this->log_(2, "ZNP TX %s cluster 0x%04X dst 0x%04X len %u", ext ? "AF_DATA_REQUEST_EXT" : "AF_DATA_REQUEST",
               f.msg.cluster, f.msg.dst, f.msg.len);
  return true;
}

bool ZnpRadio::pop(ZnpRx &out) {
  if (this->rx_.empty())
    return false;
  out = this->rx_.front();
  this->rx_.pop_front();
  return true;
}

void ZnpRadio::handle_frame_(const ZnpFrame &f, uint32_t now) {
  if (this->log_frames_) {
    char hex[3 * 24 + 4];
    size_t n = f.len < 24 ? f.len : 24, k = 0;
    for (size_t i = 0; i < n; i++)
      k += size_t(snprintf(hex + k, sizeof(hex) - k, "%02X ", f.data[i]));
    hex[k] = 0;
    this->log_(2, "ZNP RX %02X%02X len %u: %s%s", f.cmd0, f.cmd1, f.len, hex, f.len > n ? "..." : "");
  }

  // ---- start-up handshakes -----------------------------------------------------------------------------------------
  if (f.cmd0 == TYPE_AREQ_ZDO && f.cmd1 == ZDO_STATE_CHANGE_IND && f.len >= 1) {
    if (f.data[0] == DEV_ZB_COORD) {
      if (this->state_ == START_WAIT) {
        this->state_ = READY;
        this->log_(2, "CC2530 started as ZigBee coordinator (channel %u, PAN 0x%04X)", this->cfg_.channel,
                   this->cfg_.pan_id);
      }
    } else {
      this->log_(3, "ZDO state change: 0x%02X", f.data[0]);
    }
    return;
  }
  if (f.cmd0 == TYPE_AREQ_SYS && f.cmd1 == SYS_RESET_IND) {
    if (this->state_ == READY) {
      this->log_(1, "the CC2530 reset by itself, restarting the coordinator");
      this->retry_at_ = now;
      this->restart_(now);
    } else if (this->state_ == INIT_WAIT && this->init_[this->init_idx_].cmd0 == TYPE_AREQ_SYS) {
      this->log_(2, "ZNP init %u/%u %s: OK (modem restarted)", unsigned(this->init_idx_ + 1), unsigned(this->init_n_ + 1),
                 this->init_[this->init_idx_].name);
      this->first_cmd_answered_ = true;
      this->init_idx_++;
      this->state_ = INIT_SEND;
    }
    return;
  }
  if (this->state_ == INIT_WAIT && this->init_idx_ < this->init_n_) {
    const InitCmd &c = this->init_[this->init_idx_];
    // SRSP: same subsystem and command as the SREQ, type bits 0x60
    if ((f.cmd0 & 0xE0) == 0x60 && (f.cmd0 & 0x1F) == (c.cmd0 & 0x1F) && f.cmd1 == c.cmd1) {
      if (f.len >= 1 && f.data[0] != 0)
        this->log_(1, "ZNP init %u/%u %s: FAILED, status 0x%02X", unsigned(this->init_idx_ + 1),
                   unsigned(this->init_n_ + 1), c.name, f.data[0]);
      else
        this->log_(2, "ZNP init %u/%u %s: OK", unsigned(this->init_idx_ + 1), unsigned(this->init_n_ + 1), c.name);
      this->first_cmd_answered_ = true;
      this->init_idx_++;
      this->state_ = INIT_SEND;
      return;
    }
  }

  if (this->state_ == START_WAIT && f.cmd0 == 0x66 && f.cmd1 == 0x00) {
    this->log_(2, "ZNP init %u/%u ZB_START_REQUEST: accepted, waiting for the coordinator state", unsigned(this->init_n_ + 1),
               unsigned(this->init_n_ + 1));
    return;
  }

  // ---- data path -------------------------------------------------------------------------------------------------
  if (f.cmd0 == TYPE_SRSP_AF && (f.cmd1 == AF_DATA_REQUEST || f.cmd1 == AF_DATA_REQUEST_EXT)) {
    const uint8_t status = f.len >= 1 ? f.data[0] : 0xFF;
    this->tx_state_ = status == 0 ? 0 : 1 + status;
    return;
  }
  if (f.cmd0 == TYPE_AREQ_AF && f.cmd1 == AF_DATA_CONFIRM) {
    if (f.len >= 1 && f.data[0] != 0)
      this->log_(3, "AF_DATA_CONFIRM status 0x%02X (no route / no MAC ack)", f.data[0]);
    return;
  }
  if (f.cmd0 == TYPE_AREQ_AF && f.cmd1 == AF_INCOMING_MSG) {
    // GroupId(2) ClusterId(2) SrcAddr(2) SrcEP DstEP WasBroadcast LQI SecurityUse Timestamp(4) TransSeq Len Data
    if (f.len < 17 || f.len < 17u + f.data[16])
      return;
    ZnpRx m;
    m.cluster = uint16_t(f.data[2]) | (uint16_t(f.data[3]) << 8);
    m.src = uint16_t(f.data[4]) | (uint16_t(f.data[5]) << 8);
    m.was_broadcast = f.data[8] != 0;
    m.lqi = f.data[9];
    m.len = f.data[16];
    memcpy(m.data, f.data + 17, m.len);
    if (this->rx_.size() >= 16) {
      this->rx_.pop_front();
      this->dropped_++;
    }
    this->rx_.push_back(m);
  }
}

void ZnpRadio::loop(uint32_t now) {
  if (this->port_ == nullptr)
    return;

  // ---- read everything the modem has sent ------------------------------------------------------------------------------
  int b;
  ZnpFrame frame;
  for (int guard = 0; guard < 1024 && (b = this->port_->read_byte()) >= 0; guard++)
    if (this->parser_.feed(uint8_t(b), now, frame))
      this->handle_frame_(frame, now);

  // ---- start-up state machine ----------------------------------------------------------------------------------------
  switch (this->state_) {
    case IDLE:
      if (int32_t(now - this->retry_at_) >= 0) {
        this->log_(2, "ZNP: starting the CC2530 coordinator (attempt %u)", unsigned(this->init_count_ + 1));
        this->restart_(now);
      }
      break;
    case RESET_ASSERT:
      if (now - this->state_since_ >= RESET_PULSE_MS) {
        this->port_->set_reset(false);
        this->state_since_ = now;
        this->state_ = RESET_WAIT;
      }
      break;
    case RESET_WAIT:
      if (now - this->state_since_ >= RESET_SETTLE_MS) {
        this->init_idx_ = 0;
        this->state_ = INIT_SEND;
      }
      break;
    case INIT_SEND:
      if (this->init_idx_ >= this->init_n_) {
        this->send_(0x26, 0x00, nullptr, 0);  // ZB_START_REQUEST
        this->state_since_ = now;
        this->state_ = START_WAIT;
        break;
      }
      {
        const InitCmd &c = this->init_[this->init_idx_];
        if (this->log_frames_)
          this->log_(2, "ZNP init: %s", c.name);
        this->send_(c.cmd0, c.cmd1, c.data, c.len);
        this->state_since_ = now;
        this->state_ = INIT_WAIT;
      }
      break;
    case INIT_WAIT:
      if (now - this->state_since_ >= this->init_[this->init_idx_].timeout_ms) {
        const InitCmd &c = this->init_[this->init_idx_];
        if (this->init_idx_ == 0 && !this->first_cmd_answered_) {
          // the very first command is not answered: nothing is connected, wrong pins or wrong baud rate
          this->log_(0, "the CC2530 does not answer (check TX/RX wiring, baud rate 115200 and the ZNP firmware); "
                        "retrying in %u s", unsigned(RETRY_DELAY_MS / 1000));
          this->retry_at_ = now + RETRY_DELAY_MS;
          this->state_ = IDLE;
          break;
        }
        this->log_(c.cmd0 == TYPE_AREQ_SYS ? 3 : 1, "ZNP init %u/%u %s: no answer, continuing",
                   unsigned(this->init_idx_ + 1), unsigned(this->init_n_ + 1), c.name);
        this->init_idx_++;
        this->state_ = INIT_SEND;
      }
      break;
    case START_WAIT:
      if (now - this->state_since_ >= START_TIMEOUT_MS) {
        this->log_(0, "the CC2530 did not report the coordinator state, retrying in %u s",
                   unsigned(RETRY_DELAY_MS / 1000));
        this->retry_at_ = now + RETRY_DELAY_MS;
        this->state_ = IDLE;
      }
      break;
    case READY:
      break;
  }
}

}  // namespace apszb
}  // namespace esphome
