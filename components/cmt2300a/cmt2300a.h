#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include <vector>
#include <functional>

namespace esphome {
namespace cmt2300a {

// Registres CMT2300A
static const uint8_t CMT2300A_REG_MODE_CTL = 0x00;
static const uint8_t CMT2300A_REG_MODE_STA = 0x01;
static const uint8_t CMT2300A_REG_INT_FLAG = 0x02;
static const uint8_t CMT2300A_REG_INT_CLR = 0x03;
static const uint8_t CMT2300A_REG_FIFO_FLAG = 0x04;
static const uint8_t CMT2300A_REG_FIFO_CTL = 0x05;
static const uint8_t CMT2300A_REG_PKT_CTL = 0x06;

// Modes opérationnels
static const uint8_t CMT2300A_MODE_SLEEP = 0x00;
static const uint8_t CMT2300A_MODE_STBY = 0x01;
static const uint8_t CMT2300A_MODE_RX = 0x02;
static const uint8_t CMT2300A_MODE_TX = 0x03;
static const uint8_t CMT2300A_MODE_CAL = 0x04;

// Bits d'interruption
static const uint8_t CMT2300A_INT_TX_DONE = 0x01;
static const uint8_t CMT2300A_INT_RX_DONE = 0x02;
static const uint8_t CMT2300A_INT_PKT_OK = 0x04;
static const uint8_t CMT2300A_INT_CRC_ERR = 0x08;
static const uint8_t CMT2300A_INT_SYNC_DET = 0x10;

// Commandes FIFO
static const uint8_t CMT2300A_FIFO_WR = 0x00;
static const uint8_t CMT2300A_FIFO_RD = 0x80;
static const uint8_t CMT2300A_FIFO_CLR = 0x40;

// Taille maximale du FIFO
static const size_t CMT2300A_FIFO_SIZE = 255;

class CMT2300AComponent : public Component,
                          public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST,
                                               spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING,
                                               spi::DATA_RATE_1MHZ> {
 public:
  CMT2300AComponent() = default;

  // Méthodes du cycle de vie Component
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // Configuration des pins (appelées depuis Python)
  void set_cs_pin(GPIOPin *pin) { cs_pin_ = pin; }
  void set_fcs_pin(GPIOPin *pin) { fcs_pin_ = pin; }
  void set_gpio1_pin(GPIOPin *pin) { gpio1_pin_ = pin; }
  void set_gpio2_pin(GPIOPin *pin) { gpio2_pin_ = pin; }
  void set_gpio3_pin(GPIOPin *pin) { gpio3_pin_ = pin; }

  // Configuration radio (appelées depuis Python)
  void set_frequency(uint32_t freq) { frequency_ = freq; }
  void set_data_rate(uint32_t rate) { data_rate_ = rate; }
  void set_tx_power(uint8_t power) { tx_power_ = power; }
  void set_enable_crc(bool enable) { enable_crc_ = enable; }

  // API publique pour l'utilisateur
  bool send_packet(const std::vector<uint8_t> &data);
  void set_receive_callback(std::function<void(const std::vector<uint8_t> &)> callback) {
    receive_callback_ = std::move(callback);
  }
  
  // Contrôle des modes
  bool enter_rx_mode();
  bool enter_tx_mode();
  bool enter_standby_mode();
  bool enter_sleep_mode();
  
  // Getters pour état
  bool is_transmitting() const { return current_mode_ == CMT2300A_MODE_TX; }
  bool is_receiving() const { return current_mode_ == CMT2300A_MODE_RX; }
  uint8_t get_rssi();

 protected:
  // Accès registres SPI (via cs_pin_)
  void write_register_(uint8_t reg, uint8_t value);
  uint8_t read_register_(uint8_t reg);
  void write_registers_(uint8_t start_reg, const std::vector<uint8_t> &data);
  void read_registers_(uint8_t start_reg, uint8_t *data, size_t length);
  
  // Accès FIFO (via fcs_pin_)
  void write_fifo_(const std::vector<uint8_t> &data);
  std::vector<uint8_t> read_fifo_();
  void clear_fifo_();
  uint8_t get_fifo_length_();
  
  // Configuration du chip
  bool reset_chip_();
  bool configure_frequency_();
  bool configure_data_rate_();
  bool configure_packet_format_();
  bool configure_tx_power_();
  bool calibrate_();
  
  // Gestion des interruptions
  void handle_interrupt_();
  uint8_t read_interrupt_flags_();
  void clear_interrupt_flags_(uint8_t flags);
  
  // Gestion des modes
  bool set_mode_(uint8_t mode);
  uint8_t get_mode_();
  bool wait_for_mode_(uint8_t mode, uint32_t timeout_ms = 100);

  // Pins de contrôle
  GPIOPin *cs_pin_{nullptr};   // Chip Select registres
  GPIOPin *fcs_pin_{nullptr};  // FIFO Chip Select
  GPIOPin *gpio1_pin_{nullptr}; // Interruption
  GPIOPin *gpio2_pin_{nullptr}; // Usage futur
  GPIOPin *gpio3_pin_{nullptr}; // Usage futur

  // Configuration radio
  uint32_t frequency_{868000000};  // Hz
  uint32_t data_rate_{250000};     // bps
  uint8_t tx_power_{20};           // dBm
  bool enable_crc_{true};
  
  // État interne
  uint8_t current_mode_{CMT2300A_MODE_SLEEP};
  bool chip_ready_{false};
  std::function<void(const std::vector<uint8_t> &)> receive_callback_;
  
  // Statistiques
  uint32_t tx_count_{0};
  uint32_t rx_count_{0};
  uint32_t crc_error_count_{0};
};

}  // namespace cmt2300a
}  // namespace esphome