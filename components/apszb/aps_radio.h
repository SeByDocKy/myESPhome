#pragma once

#include "esphome/core/defines.h"

#ifdef USE_APSZB_NATIVE

#ifdef USE_ESP32

#include "aps_protocol.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome {
namespace apszb {

// A received MAC frame, FCS already removed.
struct RadioRx {
  uint8_t len;  // number of valid bytes in data
  int8_t rssi;
  uint8_t lqi;
  uint8_t data[MAX_PSDU];
};

// Thin wrapper around the ESP-IDF IEEE 802.15.4 radio driver (ESP32-C6 / ESP32-H2).
// The radio is used as a bare PHY/MAC: the ZigBee NWK/APS layers are implemented in aps_protocol.cpp.
class ApsRadio {
 public:
  bool begin(const NetworkConfig &cfg, int8_t tx_power_dbm, bool promiscuous);

  // Non-blocking. The frame is copied. Returns false if the radio is busy.
  bool transmit(const TxFrame &f, bool cca = true);
  // -1: transmission in progress, 0: success (MAC ack received when requested), >0: failure (see tx_error_name)
  int tx_result() const { return this->tx_state_; }
  static const char *tx_error_name(int code);

  bool pop(RadioRx &out);
  uint32_t dropped() const { return this->dropped_; }

  // called from the ISR callbacks
  void isr_rx(uint8_t *frame, int8_t rssi, uint8_t lqi);
  void isr_tx_done(bool ok, int error);

 protected:
  QueueHandle_t rx_queue_{nullptr};
  volatile int tx_state_{0};
  volatile uint32_t dropped_{0};
  uint8_t tx_buf_[MAX_PSDU + 1];
  bool started_{false};
};

}  // namespace apszb
}  // namespace esphome

#elif defined(USE_HOST)

// ---------------------------------------------------------------------------------------------------------------------
// Host stub: lets the component be compiled and unit tested on a PC (no radio)
// ---------------------------------------------------------------------------------------------------------------------
#include "aps_protocol.h"
#include <deque>
#include <vector>

namespace esphome {
namespace apszb {

struct RadioRx {
  uint8_t len;
  int8_t rssi;
  uint8_t lqi;
  uint8_t data[MAX_PSDU];
};

class ApsRadio {
 public:
  bool begin(const NetworkConfig &, int8_t, bool) { return true; }
  bool transmit(const TxFrame &f, bool = true) {
    this->sent.push_back(f);
    this->state_ = this->tx_ok ? 0 : 3;
    return true;
  }
  int tx_result() const { return this->state_; }
  static const char *tx_error_name(int code) { return code == 0 ? "ok" : "error"; }
  bool pop(RadioRx &out) {
    if (this->rx.empty())
      return false;
    out = this->rx.front();
    this->rx.pop_front();
    return true;
  }
  uint32_t dropped() const { return 0; }

  // test hooks
  std::deque<RadioRx> rx;
  std::vector<TxFrame> sent;
  bool tx_ok{true};

 protected:
  int state_{0};
};

}  // namespace apszb
}  // namespace esphome

#endif  // USE_ESP32 / USE_HOST

#endif  // USE_APSZB_NATIVE
