#include "aps_radio.h"

#if defined(USE_APSZB_NATIVE) && defined(USE_ESP32)

#include "esphome/core/log.h"

#include <esp_attr.h>
#include <esp_ieee802154.h>
#include <esp_err.h>
#include <cstring>

namespace esphome {
namespace apszb {

static const char *const TAG = "apszb.radio";

// The IEEE 802.15.4 driver reports its events through plain C callbacks that run in ISR context.
static ApsRadio *g_radio = nullptr;

bool ApsRadio::begin(const NetworkConfig &cfg, int8_t tx_power_dbm, bool promiscuous) {
  if (this->started_)
    return true;
  this->rx_queue_ = xQueueCreate(16, sizeof(RadioRx));
  if (this->rx_queue_ == nullptr) {
    ESP_LOGE(TAG, "cannot allocate the RX queue");
    return false;
  }
  g_radio = this;

  esp_err_t err = esp_ieee802154_enable();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ieee802154_enable failed: %s", esp_err_to_name(err));
    return false;
  }
  uint8_t ieee[8];
  cfg.ieee_address(ieee);

  esp_ieee802154_set_channel(cfg.channel);
  esp_ieee802154_set_txpower(tx_power_dbm);
  esp_ieee802154_set_panid(cfg.pan_id);
  esp_ieee802154_set_short_address(ADDR_COORDINATOR);
  esp_ieee802154_set_extended_address(ieee);
  esp_ieee802154_set_coordinator(true);
  esp_ieee802154_set_promiscuous(promiscuous);
  esp_ieee802154_set_rx_when_idle(true);
  err = esp_ieee802154_receive();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ieee802154_receive failed: %s", esp_err_to_name(err));
    return false;
  }
  this->started_ = true;
  ESP_LOGI(TAG, "802.15.4 radio up: channel %u, PAN 0x%04X, short address 0x%04X, %d dBm%s", cfg.channel, cfg.pan_id,
           ADDR_COORDINATOR, tx_power_dbm, promiscuous ? ", PROMISCUOUS" : "");
  return true;
}

bool ApsRadio::transmit(const TxFrame &f, bool cca) {
  if (!this->started_ || this->tx_state_ == -1)
    return false;
  memcpy(this->tx_buf_, f.buf, f.mac_len + 1);
  this->tx_state_ = -1;
  esp_err_t err = esp_ieee802154_transmit(this->tx_buf_, cca);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ieee802154_transmit: %s", esp_err_to_name(err));
    this->tx_state_ = 99;
    return false;
  }
  return true;
}

const char *ApsRadio::tx_error_name(int code) {
  switch (code) {
    case 0:
      return "ok";
    case 1 + ESP_IEEE802154_TX_ERR_CCA_BUSY:
      return "channel busy";
    case 1 + ESP_IEEE802154_TX_ERR_ABORT:
      return "aborted";
    case 1 + ESP_IEEE802154_TX_ERR_NO_ACK:
      return "no MAC ack";
    case 1 + ESP_IEEE802154_TX_ERR_INVALID_ACK:
      return "invalid ack";
    case 1 + ESP_IEEE802154_TX_ERR_COEXIST:
      return "rejected by coexistence";
    case 99:
      return "driver error";
    default:
      return "error";
  }
}

bool ApsRadio::pop(RadioRx &out) {
  if (this->rx_queue_ == nullptr)
    return false;
  return xQueueReceive(this->rx_queue_, &out, 0) == pdTRUE;
}

void IRAM_ATTR ApsRadio::isr_rx(uint8_t *frame, int8_t rssi, uint8_t lqi) {
  // frame[0] = PSDU length (including the 2 byte FCS position that the driver overwrites with RSSI/LQI)
  uint8_t plen = frame[0];
  if (plen < 7 || plen > MAX_PSDU)
    return;
  RadioRx item;
  item.len = uint8_t(plen - 2);
  item.rssi = rssi;
  item.lqi = lqi;
  memcpy(item.data, frame + 1, item.len);
  BaseType_t woken = pdFALSE;
  if (xQueueSendFromISR(this->rx_queue_, &item, &woken) != pdTRUE)
    this->dropped_ = this->dropped_ + 1;
  if (woken == pdTRUE)
    portYIELD_FROM_ISR();
}

void IRAM_ATTR ApsRadio::isr_tx_done(bool ok, int error) { this->tx_state_ = ok ? 0 : (1 + error); }

}  // namespace apszb
}  // namespace esphome

// ---------------------------------------------------------------------------------------------------------------------
// ESP-IDF IEEE 802.15.4 driver callbacks (ISR context, C linkage)
// ---------------------------------------------------------------------------------------------------------------------
extern "C" void IRAM_ATTR esp_ieee802154_receive_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info) {
  if (esphome::apszb::g_radio != nullptr && frame != nullptr)
    esphome::apszb::g_radio->isr_rx(frame, frame_info->rssi, frame_info->lqi);
  esp_ieee802154_receive_handle_done(frame);
}

extern "C" void IRAM_ATTR esp_ieee802154_transmit_done(const uint8_t * /*frame*/, const uint8_t *ack,
                                                        esp_ieee802154_frame_info_t * /*ack_frame_info*/) {
  if (ack != nullptr)
    esp_ieee802154_receive_handle_done(ack);
  if (esphome::apszb::g_radio != nullptr)
    esphome::apszb::g_radio->isr_tx_done(true, 0);
}

extern "C" void IRAM_ATTR esp_ieee802154_transmit_failed(const uint8_t * /*frame*/, esp_ieee802154_tx_error_t error) {
  if (esphome::apszb::g_radio != nullptr)
    esphome::apszb::g_radio->isr_tx_done(false, int(error));
}

#endif  // USE_APSZB_NATIVE && USE_ESP32
