#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include <vector>

namespace esphome {
namespace cmt2300a {

// Registres CMT2300A
static const uint8_t CMT2300A_REG_CHIP_MODE = 0x00;
static const uint8_t CMT2300A_REG_CHIP_STATUS = 0x01;
static const uint8_t CMT2300A_REG_INT_FLAG = 0x02;
static const uint8_t CMT2300A_REG_INT_EN = 0x03;
static const uint8_t CMT2300A_REG_FIFO_FLAG = 0x04;
static const uint8_t CMT2300A_REG_FIFO_STATUS = 0x05;

// Modes de fonctionnement
static const uint8_t CMT2300A_MODE_SLEEP = 0x00;
static const uint8_t CMT2300A_MODE_STBY = 0x01;
static const uint8_t CMT2300A_MODE_RX = 0x02;
static const uint8_t CMT2300A_MODE_TX = 0x03;

// Commandes FIFO
static const uint8_t CMT2300A_FIFO_WRITE = 0x00;
static const uint8_t CMT2300A_FIFO_READ = 0x80;
static const uint8_t CMT2300A_FIFO_CLEAR = 0x40;

// Interruptions
static const uint8_t CMT2300A_INT_RX_DONE = 0x01;
static const uint8_t CMT2300A_INT_TX_DONE = 0x02;
static const uint8_t CMT2300A_INT_PKT_OK = 0x04;
static const uint8_t CMT2300A_INT_CRC_ERROR = 0x08;

class CMT2300AComponent : public Component, public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, 
                                                                   spi::CLOCK_POLARITY_LOW,
                                                                   spi::CLOCK_PHASE_LEADING,
                                                                   spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_cs_pin(GPIOPin *pin) { cs_pin_ = pin; }
  void set_fcs_pin(GPIOPin *pin) { fcs_pin_ = pin; }
  void set_gpio1_pin(GPIOPin *pin) { gpio1_pin_ = pin; }
  void set_gpio2_pin(GPIOPin *pin) { gpio2_pin_ = pin; }
  void set_gpio3_pin(GPIOPin *pin) { gpio3_pin_ = pin; }
  void set_frequency(uint32_t freq) { frequency_ = freq; }
  void set_data_rate(uint32_t rate) { data_rate_ = rate; }

  // API publique
  bool send_packet(const std::vector<uint8_t> &data);
  void set_receive_callback(std::function<void(const std::vector<uint8_t> &)> callback) {
    receive_callback_ = callback;
  }
  void enter_rx_mode();
  void enter_tx_mode();
  void enter_standby_mode();

 protected:
  // Fonctions de bas niveau
  void write_register_(uint8_t reg, uint8_t value);
  uint8_t read_register_(uint8_t reg);
  void write_fifo_(const std::vector<uint8_t> &data);
  std::vector<uint8_t> read_fifo_(uint8_t length);
  void clear_fifo_();
  
  // Configuration
  void configure_frequency_();
  void configure_data_rate_();
  void configure_packet_();
  void reset_chip_();
  
  // Gestion des interruptions
  void handle_interrupt_();
  uint8_t read_interrupt_flags_();
  void clear_interrupt_flags_();

  GPIOPin *cs_pin_{nullptr};
  GPIOPin *fcs_pin_{nullptr};
  GPIOPin *gpio1_pin_{nullptr};
  GPIOPin *gpio2_pin_{nullptr};
  GPIOPin *gpio3_pin_{nullptr};
  
  uint32_t frequency_{868000000};
  uint32_t data_rate_{250000};
  
  std::function<void(const std::vector<uint8_t> &)> receive_callback_;
};

}  // namespace cmt2300a
}  // namespace esphome
}  // namespace esphome
