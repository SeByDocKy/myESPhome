#pragma once

#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace nrf24l01 {

// ---------------------------------------------------------------------------
// Registres et commandes portés depuis nRF24L01.h (Stefan Engelke / TMRh20
// nRF24/RF24 -- carte mémoire du datasheet nRF24L01+, stable depuis 15+ ans,
// recoupée sur plusieurs implémentations indépendantes).
// ---------------------------------------------------------------------------
static const uint8_t REG_CONFIG = 0x00;
static const uint8_t REG_EN_AA = 0x01;
static const uint8_t REG_EN_RXADDR = 0x02;
static const uint8_t REG_SETUP_AW = 0x03;
static const uint8_t REG_SETUP_RETR = 0x04;
static const uint8_t REG_RF_CH = 0x05;
static const uint8_t REG_RF_SETUP = 0x06;
static const uint8_t REG_STATUS = 0x07;
static const uint8_t REG_OBSERVE_TX = 0x08;
static const uint8_t REG_RPD = 0x09;
static const uint8_t REG_RX_ADDR_P0 = 0x0A;
static const uint8_t REG_RX_ADDR_P1 = 0x0B;
static const uint8_t REG_RX_ADDR_P2 = 0x0C;
static const uint8_t REG_RX_ADDR_P3 = 0x0D;
static const uint8_t REG_RX_ADDR_P4 = 0x0E;
static const uint8_t REG_RX_ADDR_P5 = 0x0F;
static const uint8_t REG_TX_ADDR = 0x10;
static const uint8_t REG_RX_PW_P0 = 0x11;
static const uint8_t REG_FIFO_STATUS = 0x17;
static const uint8_t REG_DYNPD = 0x1C;
static const uint8_t REG_FEATURE = 0x1D;

// CONFIG
static const uint8_t MASK_RX_DR = 1 << 6;
static const uint8_t MASK_TX_DS = 1 << 5;
static const uint8_t MASK_MAX_RT = 1 << 4;
static const uint8_t MASK_EN_CRC = 1 << 3;
static const uint8_t MASK_CRCO = 1 << 2;
static const uint8_t MASK_PWR_UP = 1 << 1;
static const uint8_t MASK_PRIM_RX = 1 << 0;

// STATUS
static const uint8_t STATUS_RX_DR = 1 << 6;
static const uint8_t STATUS_TX_DS = 1 << 5;
static const uint8_t STATUS_MAX_RT = 1 << 4;
static const uint8_t STATUS_RX_P_NO_MASK = 0x0E;
static const uint8_t STATUS_TX_FULL = 1 << 0;

// FIFO_STATUS
static const uint8_t FIFO_TX_FULL = 1 << 5;
static const uint8_t FIFO_TX_EMPTY = 1 << 4;
static const uint8_t FIFO_RX_FULL = 1 << 1;
static const uint8_t FIFO_RX_EMPTY = 1 << 0;

// FEATURE
static const uint8_t FEATURE_EN_DPL = 1 << 2;
static const uint8_t FEATURE_EN_ACK_PAY = 1 << 1;
static const uint8_t FEATURE_EN_DYN_ACK = 1 << 0;

// Commandes SPI
static const uint8_t CMD_R_REGISTER = 0x00;
static const uint8_t CMD_W_REGISTER = 0x20;
static const uint8_t CMD_REGISTER_MASK = 0x1F;
static const uint8_t CMD_ACTIVATE = 0x50;
static const uint8_t CMD_R_RX_PL_WID = 0x60;
static const uint8_t CMD_R_RX_PAYLOAD = 0x61;
static const uint8_t CMD_W_TX_PAYLOAD = 0xA0;
static const uint8_t CMD_W_ACK_PAYLOAD = 0xA8;
static const uint8_t CMD_FLUSH_TX = 0xE1;
static const uint8_t CMD_FLUSH_RX = 0xE2;
static const uint8_t CMD_REUSE_TX_PL = 0xE3;
static const uint8_t CMD_NOP = 0xFF;

enum PALevel : uint8_t { PA_MIN = 0, PA_LOW = 1, PA_HIGH = 2, PA_MAX = 3 };
enum DataRate : uint8_t { RATE_1MBPS = 0, RATE_2MBPS = 1, RATE_250KBPS = 2 };
enum CRCLength : uint8_t { CRC_DISABLED = 0, CRC_8BIT = 1, CRC_16BIT = 2 };

class NRF24Component : public Component,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_4MHZ> {
 public:
  void set_ce_pin(GPIOPin *pin) { this->ce_pin_ = pin; }
  void set_irq_pin(InternalGPIOPin *pin) { this->irq_pin_ = pin; }

  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_pa_level(uint8_t level) { this->pa_level_ = static_cast<PALevel>(level); }
  void set_data_rate(uint8_t rate) { this->data_rate_ = static_cast<DataRate>(rate); }
  void set_crc_length(uint8_t len) { this->crc_length_ = static_cast<CRCLength>(len); }
  void set_address_width(uint8_t width) { this->address_width_ = width; }
  void set_auto_ack(bool enable) { this->auto_ack_ = enable; }
  void set_retry_delay(uint8_t d) { this->retry_delay_ = d; }
  void set_retry_count(uint8_t c) { this->retry_count_ = c; }
  void set_payload_size(uint8_t size) { this->payload_size_ = size; }
  void set_dynamic_payloads(bool enable) { this->dynamic_payloads_ = enable; }
  void set_tx_address(const std::vector<uint8_t> &addr) {
    for (uint8_t i = 0; i < 5 && i < addr.size(); i++) this->tx_address_[i] = addr[i];
  }
  void set_rx_address(const std::vector<uint8_t> &addr) {
    for (uint8_t i = 0; i < 5 && i < addr.size(); i++) this->rx_address_[i] = addr[i];
  }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Envoie un paquet (jusqu'à payload_size octets). Bloquant jusqu'à TX_DS/MAX_RT
  /// ou timeout_ms -- une transmission nRF24 est de l'ordre de quelques centaines de
  /// microsecondes à quelques millisecondes selon data_rate/retries, donc un court
  /// blocage ici est sans commune mesure avec le CMT2300A bit-bangé.
  bool send_packet(const std::vector<uint8_t> &data, uint32_t timeout_ms = 100);

  void add_on_packet_received_callback(std::function<void(std::vector<uint8_t>)> cb) {
    this->packet_callback_.add(std::move(cb));
  }

 protected:
  // --- Accès registres bas niveau -- port de RF24::read_register/write_register ---
  uint8_t read_register_(uint8_t reg);
  void read_register_(uint8_t reg, uint8_t *buf, uint8_t len);
  uint8_t write_register_(uint8_t reg, uint8_t value);
  uint8_t write_register_(uint8_t reg, const uint8_t *buf, uint8_t len);
  uint8_t get_status_();
  void flush_rx_();
  void flush_tx_();

  // --- Séquence haut niveau -- port de RF24::begin()/setXxx()/startListening()/... ---
  void power_up_();
  void apply_pa_level_();
  void apply_data_rate_();
  void apply_crc_length_();
  void apply_retries_();
  void apply_address_width_();
  void open_writing_pipe_();
  void open_reading_pipe_(uint8_t pipe, const uint8_t *addr);
  void start_listening_();
  void stop_listening_();
  bool write_payload_(const uint8_t *buf, uint8_t len);
  uint8_t read_payload_(uint8_t *buf, uint8_t maxlen);
  bool rx_available_();

  GPIOPin *ce_pin_{nullptr};
  InternalGPIOPin *irq_pin_{nullptr};

  uint8_t channel_{76};
  PALevel pa_level_{PA_MAX};
  DataRate data_rate_{RATE_1MBPS};
  CRCLength crc_length_{CRC_16BIT};
  uint8_t address_width_{5};
  bool auto_ack_{true};
  uint8_t retry_delay_{5};
  uint8_t retry_count_{15};
  uint8_t payload_size_{32};
  bool dynamic_payloads_{false};
  uint8_t tx_address_[5]{0xE7, 0xE7, 0xE7, 0xE7, 0xE7};
  uint8_t rx_address_[5]{0xE7, 0xE7, 0xE7, 0xE7, 0xE7};

  bool listening_{false};
  bool setup_failed_{false};

  CallbackManager<void(std::vector<uint8_t>)> packet_callback_{};
};

template<typename... Ts> class NRF24SendAction : public Action<Ts...> {
 public:
  NRF24SendAction(NRF24Component *parent) : parent_(parent) {}

  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) {
    this->data_func_ = std::move(func);
    this->static_ = false;
  }
  void set_data_static(const std::vector<uint8_t> &data) {
    this->data_static_ = data;
    this->static_ = true;
  }

  void play(Ts... x) override {
    std::vector<uint8_t> data = this->static_ ? this->data_static_ : this->data_func_(x...);
    this->parent_->send_packet(data);
  }

 protected:
  NRF24Component *parent_;
  bool static_{true};
  std::vector<uint8_t> data_static_{};
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
};

class NRF24PacketReceivedTrigger : public Trigger<std::vector<uint8_t>> {
 public:
  explicit NRF24PacketReceivedTrigger(NRF24Component *parent) {
    parent->add_on_packet_received_callback([this](std::vector<uint8_t> data) { this->trigger(data); });
  }
};

}  // namespace nrf24l01
}  // namespace esphome
