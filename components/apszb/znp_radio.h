#pragma once

// ZNP (Z-Stack "Network Processor") backend of the "apszb" component.
//
// A TI CC2530 running the ZNP firmware is connected to an UART. The CC2530 does the IEEE 802.15.4 / ZigBee work, the host only
// exchanges ZNP frames with it, exactly like the original ESP32-ECU project:
//
//     FE | len | cmd0 | cmd1 | data (len bytes) | fcs        fcs = XOR of len, cmd0, cmd1 and data
//
// This file has no ESPHome dependency (the UART is reached through ZnpPort) so it can be unit tested on a PC.

#include "aps_protocol.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <initializer_list>

namespace esphome {
namespace apszb {

// ---------------------------------------------------------------------------------------------------------------------
// ZNP framing
// ---------------------------------------------------------------------------------------------------------------------
static const size_t ZNP_MAX_DATA = 250;

struct ZnpFrame {
  uint8_t cmd0 = 0;
  uint8_t cmd1 = 0;
  uint8_t len = 0;
  uint8_t data[ZNP_MAX_DATA];
};

// Encode a ZNP frame into `out` (must hold len + 5 bytes). Returns the number of bytes written.
size_t znp_encode(uint8_t cmd0, uint8_t cmd1, const uint8_t *data, size_t len, uint8_t *out);

// Byte oriented stream parser. Resynchronises on a bad checksum or when a frame stalls.
class ZnpParser {
 public:
  // Returns true when `out` holds a complete, valid frame.
  bool feed(uint8_t b, uint32_t now_ms, ZnpFrame &out);
  uint32_t bad_frames() const { return this->bad_; }

 protected:
  enum State : uint8_t { SOF, LEN, CMD0, CMD1, DATA, FCS } state_{SOF};
  uint8_t fcs_{0};
  size_t pos_{0};
  ZnpFrame cur_;
  uint32_t last_byte_{0};
  uint32_t bad_{0};
};

// ---------------------------------------------------------------------------------------------------------------------
// Access to the UART (and to the optional RESET line of the CC2530)
// ---------------------------------------------------------------------------------------------------------------------
class ZnpPort {
 public:
  virtual ~ZnpPort() = default;
  virtual void write(const uint8_t *data, size_t len) = 0;
  virtual int read_byte() = 0;                // -1 when nothing is available
  virtual void set_reset(bool /*asserted*/) {}    // optional hardware reset line (active = module held in reset)
  virtual bool has_reset() const { return false; }
};

// A message delivered by the CC2530 (AF_INCOMING_MSG)
struct ZnpRx {
  uint16_t src = 0;
  uint16_t cluster = 0;
  uint8_t lqi = 0;
  bool was_broadcast = false;
  uint8_t len = 0;
  uint8_t data[ZNP_MAX_DATA];
};

// ---------------------------------------------------------------------------------------------------------------------
// Radio backend
// ---------------------------------------------------------------------------------------------------------------------
class ZnpRadio {
 public:
  using LogFn = void (*)(int level, const char *msg);  // level: 0 error, 1 warning, 2 info, 3 debug, 4 verbose

  void set_port(ZnpPort *port) { this->port_ = port; }
  void set_logger(LogFn fn) { this->log_fn_ = fn; }
  void set_log_frames(bool v) { this->log_frames_ = v; }

  // Prepare the coordinator configuration (ECU id, PAN, channel). The start-up sequence runs from loop().
  void begin(const NetworkConfig &cfg);
  void loop(uint32_t now_ms);

  bool ready() const { return this->state_ == READY; }
  // Non-blocking. Uses f.msg. Returns false when the modem is not ready or busy.
  bool transmit(const TxFrame &f);
  // -1: waiting for the modem, 0: accepted by the CC2530, >0: failure (see tx_error_name)
  int tx_result() const { return this->tx_state_; }
  static const char *tx_error_name(int code);

  bool pop(ZnpRx &out);
  uint32_t dropped() const { return this->dropped_; }
  uint32_t init_count() const { return this->init_count_; }

 protected:
  enum State : uint8_t { IDLE, RESET_ASSERT, RESET_WAIT, INIT_SEND, INIT_WAIT, START_WAIT, READY } state_{IDLE};

  void restart_(uint32_t now);
  void send_(uint8_t cmd0, uint8_t cmd1, const uint8_t *data, size_t len);
  void handle_frame_(const ZnpFrame &f, uint32_t now);
  void log_(int level, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

  ZnpPort *port_{nullptr};
  LogFn log_fn_{nullptr};
  bool log_frames_{false};
  NetworkConfig cfg_;
  ZnpParser parser_;

  // start-up sequence
  struct InitCmd {
    uint8_t cmd0, cmd1;
    uint8_t len;
    uint8_t data[16];
    const char *name;
    uint16_t timeout_ms;
  };
  static const size_t MAX_INIT = 10;
  InitCmd init_[MAX_INIT];
  size_t init_n_{0};
  size_t init_idx_{0};
  uint32_t state_since_{0};
  uint32_t retry_at_{0};
  uint32_t init_count_{0};
  bool first_cmd_answered_{false};

  // TX bookkeeping
  volatile int tx_state_{0};
  uint8_t trans_id_{0};

  std::deque<ZnpRx> rx_;
  uint32_t dropped_{0};
};

}  // namespace apszb
}  // namespace esphome
