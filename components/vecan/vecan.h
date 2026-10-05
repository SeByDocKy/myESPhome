#pragma once

#include "esphome/core/component.h"
#include "esphome/components/canbus/canbus.h"
#include "vecan_proto.h"

#include <map>
#include <vector>

namespace esphome {
namespace vecan {

class VeCanHub;

/// Base class for everything that talks to ONE Victron device on the bus (identified by its source address).
/// The hub only forwards frames whose source address matches address().
class VeCanDevice {
 public:
  virtual ~VeCanDevice() = default;

  void set_address(uint8_t address) { this->address_ = address; }
  uint8_t get_address() const { return this->address_; }

  /// Standard (non VREG) single-frame PGN received from this device.
  virtual void on_pgn(uint32_t /*pgn*/, const uint8_t * /*data*/, uint8_t /*len*/) {}
  /// VREG value broadcast by this device (answer to a request, or change notification).
  /// `data` points after the 4-byte header (66 99 regL regH); single frames always carry 4 bytes (zero padded).
  virtual void on_vreg(uint16_t /*reg*/, const uint8_t * /*data*/, uint16_t /*len*/) {}
  /// The device refused a request (ACK/NACK message, error code >= 0x8000).
  virtual void on_vreg_nack(uint16_t /*reg*/, uint16_t /*code*/) {}

 protected:
  uint8_t address_{ADDR_GLOBAL};
};

/// VE.Can hub: owns the address claim, the paced transmit queue, VREG framing and fast-packet reassembly.
/// It sits on top of any ESPHome `canbus` platform (mcp2515, mcp2518fd, esp32_can, ...).
class VeCanHub : public Component {
 public:
  void set_canbus(canbus::Canbus *canbus) { this->canbus_ = canbus; }
  void set_address(uint8_t address) { this->preferred_address_ = address; }
  void set_listen_only(bool listen_only) { this->listen_only_ = listen_only; }
  void set_tx_interval(uint32_t ms) { this->tx_interval_ms_ = ms; }

  void register_device(VeCanDevice *device) { this->devices_.push_back(device); }

  /// Queue a request for one VREG of the device at `dst`. Duplicates already waiting are dropped.
  /// Returns false when nothing can be sent (listen-only, queue full).
  /// With a mask other than 0xFFFF every register whose id AND mask equals `reg` answers: request_vreg(dst, 0x2200,
  /// 0xFF00) asks for the whole page 0x22. Used to discover what a device implements.
  bool request_vreg(uint8_t dst, uint16_t reg, uint16_t mask = 0xFFFF);

  /// Queue a write of one VREG (single frame, up to 4 data bytes, little endian) of the device at `dst`.
  /// A write still waiting for the same register is replaced by the new value. Writes are sent before queued reads.
  /// The device answers with a broadcast of the new value, or with a NACK (see VeCanDevice::on_vreg_nack).
  /// Returns false when nothing can be sent (listen-only, queue full).
  bool write_vreg(uint8_t dst, uint16_t reg, uint32_t value);

  /// True once our address is claimed and transmissions are allowed.
  bool can_transmit() const { return this->state_ == State::READY; }
  bool is_listen_only() const { return this->listen_only_; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  enum class State : uint8_t { INIT, WAIT_SEEN, CLAIMING, READY, LISTEN_ONLY };

  struct TxItem {
    uint8_t dst;
    uint16_t reg;
    bool write;
    uint32_t value;  // write: the value, read: the request mask
  };

  void on_frame_(uint32_t can_id, bool extended, bool rtr, const std::vector<uint8_t> &data);
  void handle_address_claim_(uint8_t src, const uint8_t *data, uint8_t len);
  void handle_vreg_(uint8_t src, const uint8_t *payload, size_t len);

  void send_frame_(uint32_t id, const uint8_t *data, uint8_t len);
  void send_address_claim_();
  void send_claim_request_();
  void choose_address_();

  canbus::Canbus *canbus_{nullptr};
  std::vector<VeCanDevice *> devices_;

  State state_{State::INIT};
  bool listen_only_{false};
  uint8_t preferred_address_{0xA0};
  uint8_t address_{0xA0};
  uint64_t name_{0};
  uint32_t boot_ms_{0};
  uint32_t state_ms_{0};
  uint32_t last_tx_ms_{0};
  uint32_t tx_interval_ms_{20};

  std::vector<TxItem> tx_queue_;
  std::map<uint8_t, uint64_t> seen_;  // source address -> NAME of every node that announced itself
  FastPacketAssembler assembler_;
};

}  // namespace vecan
}  // namespace esphome
