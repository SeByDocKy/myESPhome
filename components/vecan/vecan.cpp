#include "vecan.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace vecan {

static const char *const TAG = "vecan";

static const uint32_t WAIT_BEFORE_REQUEST_MS = 500;  // let the bus settle after boot
static const uint32_t WAIT_FOR_CLAIMS_MS = 1500;     // collect the address claims of the other nodes
static const uint32_t CLAIM_HOLD_MS = 250;           // ISO 11783-5: wait 250 ms before using a claimed address
static const size_t TX_QUEUE_MAX = 48;
static const uint8_t PRIORITY_VREG = 7;  // Victron: proprietary messages use priority 7
static const uint8_t PRIORITY_NETWORK = 6;
static const uint8_t NAME_FUNCTION_NOT_AVAILABLE = 0xFF;

void VeCanHub::setup() {
  if (this->canbus_ == nullptr) {
    ESP_LOGE(TAG, "No canbus configured");
    this->mark_failed();
    return;
  }

  // NAME: unique identity from the MAC address; 2047 is the "not registered" manufacturer code.
  uint64_t mac = 0;
  uint8_t mac_bytes[6];
  get_mac_address_raw(mac_bytes);
  for (int i = 0; i < 6; i++)
    mac = (mac << 8) | mac_bytes[i];
  this->name_ = make_name(static_cast<uint32_t>(mac & 0x1FFFFF), 2047, NAME_FUNCTION_NOT_AVAILABLE);

  this->address_ = this->preferred_address_;
  this->boot_ms_ = millis();
  this->state_ = this->listen_only_ ? State::LISTEN_ONLY : State::INIT;

  this->canbus_->add_callback([this](uint32_t can_id, bool extended, bool rtr, const std::vector<uint8_t> &data) {
    this->on_frame_(can_id, extended, rtr, data);
  });
}

void VeCanHub::dump_config() {
  ESP_LOGCONFIG(TAG, "VE.Can hub:");
  ESP_LOGCONFIG(TAG, "  Preferred address: 0x%02X", this->preferred_address_);
  ESP_LOGCONFIG(TAG, "  Listen only: %s", YESNO(this->listen_only_));
  ESP_LOGCONFIG(TAG, "  TX interval: %" PRIu32 " ms", this->tx_interval_ms_);
  if (this->state_ == State::READY)
    ESP_LOGCONFIG(TAG, "  Claimed address: 0x%02X", this->address_);
}

void VeCanHub::loop() {
  uint32_t now = millis();
  switch (this->state_) {
    case State::INIT:
      if (now - this->boot_ms_ >= WAIT_BEFORE_REQUEST_MS) {
        this->send_claim_request_();
        this->state_ = State::WAIT_SEEN;
        this->state_ms_ = now;
      }
      break;
    case State::WAIT_SEEN:
      if (now - this->state_ms_ >= WAIT_FOR_CLAIMS_MS) {
        this->choose_address_();
        this->send_address_claim_();
        this->state_ = State::CLAIMING;
        this->state_ms_ = now;
      }
      break;
    case State::CLAIMING:
      if (now - this->state_ms_ >= CLAIM_HOLD_MS) {
        this->state_ = State::READY;
        ESP_LOGI(TAG, "Address 0x%02X claimed, ready to transmit", this->address_);
      }
      break;
    case State::READY:
      if (!this->tx_queue_.empty() && now - this->last_tx_ms_ >= this->tx_interval_ms_) {
        TxItem item = this->tx_queue_.front();
        this->tx_queue_.erase(this->tx_queue_.begin());
        uint8_t payload[8];
        build_vreg_request(item.reg, payload);
        this->send_frame_(encode_id(PRIORITY_VREG, PGN_VREG_SF, item.dst, this->address_), payload, 8);
        this->last_tx_ms_ = now;
      }
      break;
    case State::LISTEN_ONLY:
      break;
  }
}

bool VeCanHub::request_vreg(uint8_t dst, uint16_t reg) {
  if (this->state_ == State::LISTEN_ONLY)
    return false;
  for (const auto &q : this->tx_queue_) {
    if (q.dst == dst && q.reg == reg)
      return true;  // already waiting
  }
  if (this->tx_queue_.size() >= TX_QUEUE_MAX) {
    ESP_LOGW(TAG, "TX queue full, dropping request for register 0x%04X", reg);
    return false;
  }
  this->tx_queue_.push_back({dst, reg});
  return true;
}

void VeCanHub::send_frame_(uint32_t id, const uint8_t *data, uint8_t len) {
  if (this->listen_only_)
    return;
  std::vector<uint8_t> payload(data, data + len);
  this->canbus_->send_data(id, true, false, payload);
}

void VeCanHub::send_claim_request_() {
  // ISO Request (PGN 0xEA00) addressed to everybody, asking for PGN 0xEE00 (address claim)
  uint8_t d[3] = {static_cast<uint8_t>(PGN_ISO_ADDRESS_CLAIM & 0xFF), static_cast<uint8_t>((PGN_ISO_ADDRESS_CLAIM >> 8) & 0xFF),
                  static_cast<uint8_t>((PGN_ISO_ADDRESS_CLAIM >> 16) & 0xFF)};
  this->send_frame_(encode_id(PRIORITY_NETWORK, PGN_ISO_REQUEST, ADDR_GLOBAL, ADDR_NULL), d, 3);
}

void VeCanHub::send_address_claim_() {
  uint8_t d[8];
  for (int i = 0; i < 8; i++)
    d[i] = (this->name_ >> (8 * i)) & 0xFF;
  this->send_frame_(encode_id(PRIORITY_NETWORK, PGN_ISO_ADDRESS_CLAIM, ADDR_GLOBAL, this->address_), d, 8);
}

void VeCanHub::choose_address_() {
  uint8_t addr = this->preferred_address_;
  for (int tries = 0; tries < 128; tries++) {
    auto it = this->seen_.find(addr);
    // free, or held by a node with a higher NAME (it has to give way to us)
    if (it == this->seen_.end() || it->second > this->name_)
      break;
    ESP_LOGW(TAG, "Address 0x%02X is used by another node, trying the next one", addr);
    addr = addr >= 247 ? 128 : addr + 1;
  }
  this->address_ = addr;
}

void VeCanHub::on_frame_(uint32_t can_id, bool extended, bool rtr, const std::vector<uint8_t> &data) {
  if (!extended || rtr)
    return;
  CanId id = decode_id(can_id);
  const uint8_t *d = data.data();
  uint8_t len = static_cast<uint8_t>(data.size());

  switch (id.pgn) {
    case PGN_ISO_ADDRESS_CLAIM:
      this->handle_address_claim_(id.src, d, len);
      return;
    case PGN_ISO_REQUEST:
      // someone asks for address claims: answer if the request is for us (or global) and we own an address
      if (len >= 3 && (id.dst == ADDR_GLOBAL || id.dst == this->address_) &&
          (d[0] | (d[1] << 8) | (d[2] << 16)) == static_cast<int>(PGN_ISO_ADDRESS_CLAIM) &&
          (this->state_ == State::CLAIMING || this->state_ == State::READY)) {
        this->send_address_claim_();
      }
      return;
    case PGN_VREG_SF:
      if (id.dst == ADDR_GLOBAL || id.dst == this->address_)
        this->handle_vreg_(id.src, d, len);
      return;
    case PGN_VREG_FP:
      if ((id.dst == ADDR_GLOBAL || id.dst == this->address_) && this->assembler_.add_frame(id.src, d, len))
        this->handle_vreg_(id.src, this->assembler_.data(), this->assembler_.size());
      return;
    default:
      break;
  }

  for (auto *dev : this->devices_) {
    if (dev->get_address() == id.src)
      dev->on_pgn(id.pgn, d, len);
  }
}

void VeCanHub::handle_address_claim_(uint8_t src, const uint8_t *data, uint8_t len) {
  if (len < 8)
    return;
  uint64_t name = rd_u64(data);

  if (src == this->address_ && (this->state_ == State::CLAIMING || this->state_ == State::READY) &&
      name != this->name_) {
    if (name < this->name_) {
      ESP_LOGW(TAG, "Address 0x%02X claimed by a node with higher priority, moving to another address", src);
      this->seen_[src] = name;
      this->choose_address_();
      this->send_address_claim_();
      this->state_ = State::CLAIMING;
      this->state_ms_ = millis();
      this->tx_queue_.clear();
    } else {
      this->send_address_claim_();  // we win: re-assert
    }
    return;
  }

  bool is_new = this->seen_.find(src) == this->seen_.end();
  this->seen_[src] = name;
  if (is_new && name_manufacturer(name) == VICTRON_MANUFACTURER_CODE) {
    ESP_LOGI(TAG, "Victron device found at address 0x%02X (NAME function %u)", src, name_function(name));
  }
}

void VeCanHub::handle_vreg_(uint8_t src, const uint8_t *payload, size_t len) {
  VregMsg msg;
  if (!parse_vreg(payload, len, msg))
    return;

  if (msg.reg == VREG_ACK) {
    // payload: un16 regId, un16 ackCode (low byte first). ACK code 0 is the plain "ok" and is of no interest.
    if (msg.len < 4)
      return;
    uint16_t reg = rd_u16(msg.data);
    uint16_t code = rd_u16(msg.data + 2);
    if (code < 0x8000) {
      return;
    }
    ESP_LOGD(TAG, "NACK from 0x%02X for register 0x%04X, code 0x%04X", src, reg, code);
    for (auto *dev : this->devices_) {
      if (dev->get_address() == src)
        dev->on_vreg_nack(reg, code);
    }
    return;
  }

  for (auto *dev : this->devices_) {
    if (dev->get_address() == src)
      dev->on_vreg(msg.reg, msg.data, msg.len);
  }
}

}  // namespace vecan
}  // namespace esphome
