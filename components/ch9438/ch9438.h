/// @file ch9438.h
/// @brief WCH CH9438 (SPI to 8 UART + 8 GPIO) - classes declaration
/// @details The hub (CH9438Component) owns the SPI link and the pin multiplexing. Each configured UART is a
/// CH9438Channel (an esphome::uart::UARTComponent, so any UART consumer can use it) and each GPIO is exposed
/// through CH9438GPIOPin.

#pragma once
#include <algorithm>
#include <array>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "ch9438_defs.h"

namespace esphome::ch9438 {

class CH9438Component;

/// @brief Software ring buffer in which the bytes read from a chip FIFO are stored until the consumer takes them.
/// @details The capacity is chosen at run time. The chip FIFO is read directly into the ring (write_ptr / commit)
/// so that no intermediate copy is needed.
class RxRing {
 public:
  void init(size_t capacity) {
    this->buf_.assign(capacity, 0);
    this->clear();
  }
  size_t capacity() const { return this->buf_.size(); }
  size_t count() const { return this->count_; }
  size_t free() const { return this->buf_.size() - this->count_; }
  bool is_full() const { return this->count_ == this->buf_.size(); }
  bool is_empty() const { return this->count_ == 0; }
  void clear() { this->head_ = this->tail_ = this->count_ = 0; }

  /// @brief pointer to the next free slot; up to contiguous_free() bytes can be written there, then commit() them
  uint8_t *write_ptr() { return this->buf_.data() + this->head_; }
  size_t contiguous_free() const {
    return std::min(this->buf_.size() - this->count_, this->buf_.size() - this->head_);
  }
  void commit(size_t n) {
    this->head_ = (this->head_ + n) % this->buf_.size();
    this->count_ += n;
  }
  bool peek(uint8_t &value) const {
    if (this->count_ == 0)
      return false;
    value = this->buf_[this->tail_];
    return true;
  }
  /// @brief removes up to n bytes and copies them to dst
  size_t pop_bulk(uint8_t *dst, size_t n) {
    n = std::min(n, this->count_);
    size_t const first = std::min(n, this->buf_.size() - this->tail_);
    std::copy_n(this->buf_.data() + this->tail_, first, dst);
    std::copy_n(this->buf_.data(), n - first, dst + first);
    this->tail_ = (this->tail_ + n) % this->buf_.size();
    this->count_ -= n;
    if (this->count_ == 0)  // empty: restart at the beginning so that the next bulk write stays contiguous
      this->head_ = this->tail_ = 0;
    return n;
  }

 protected:
  std::vector<uint8_t> buf_;
  size_t head_{0};
  size_t tail_{0};
  size_t count_{0};
};

/// @brief One UART of the chip, seen by ESPHome as a regular UARTComponent
class CH9438Channel : public uart::UARTComponent {
 public:
  void set_parent(CH9438Component *parent);
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  uint8_t get_channel() const { return this->channel_; }
  void set_channel_name(const char *name) { this->name_ = name; }
  const char *get_channel_name() const { return this->name_; }
  void set_rx_buffer_size(size_t size) { this->rx_buffer_size_ = size; }
  void set_flow_control(bool enabled) { this->flow_control_ = enabled; }

  /// @brief initialise the chip registers of this UART (FIFO reset, line settings, baud rate, interrupts)
  void setup_channel();
  void dump_channel();

  // uart::UARTComponent interface
  void write_array(const uint8_t *buffer, size_t length) override;
  bool read_array(uint8_t *buffer, size_t length) override;
  bool peek_byte(uint8_t *buffer) override;
  size_t available() override;
  uart::UARTFlushResult flush() override;
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {
    this->set_line_param_();
    this->set_baudrate_();
  }
  using UARTComponent::load_settings;
#endif

 protected:
  friend class CH9438Component;

  void check_logger_conflict() override {}
  void set_line_param_();
  void set_baudrate_();
  /// @brief number of bytes that can still be written in the chip transmit FIFO
  size_t tx_free_();
  /// @brief move up to `count` bytes from the chip receive FIFO to the ring buffer
  size_t xfer_fifo_to_buffer_(size_t count);
  uint8_t reg_addr_(uint8_t reg) const { return static_cast<uint8_t>(this->channel_ * 8 + reg); }

  RxRing receive_buffer_;
  CH9438Component *parent_{nullptr};
  uint8_t channel_{0};
  const char *name_{""};
  size_t rx_buffer_size_{256};
  bool flow_control_{false};
  uint8_t lcr_{LCR_WORD_8};  ///< line control register value currently programmed
  uint32_t last_overflow_log_{0};
};

/// @brief One of the 8 GPIO of the chip
class CH9438GPIOPin : public GPIOPin {
 public:
  void set_parent(CH9438Component *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  size_t dump_summary(char *buffer, size_t len) const override;
  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  CH9438Component *parent_{nullptr};
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{gpio::FLAG_NONE};
};

/// @brief The chip itself: SPI link, pin multiplexing, clock, interrupt line and status polling
class CH9438Component : public Component,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                              spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_4MHZ> {
 public:
  // configuration (called by the generated code)
  void set_crystal(bool crystal) { this->crystal_ = crystal; }
  void set_irq_pin(InternalGPIOPin *pin) { this->irq_pin_ = pin; }
  void set_tnow_mask(uint8_t enable, uint8_t invert) {
    this->tnow_mask_ = enable;
    this->tnow_invert_ = invert;
  }
  void set_cts_mask(uint8_t mask) { this->cts_mask_ = mask; }
  void set_rts_mask(uint8_t mask) { this->rts_mask_ = mask; }
  void set_name(const char *name) { this->name_ = name; }
  const char *get_name() const { return this->name_; }
#ifdef USE_TEXT_SENSOR
  void set_version_text_sensor(text_sensor::TextSensor *sensor) { this->version_text_sensor_ = sensor; }
#endif

  void setup() override;
  void loop() override;
  void dump_config() override;
  /// The hub sits just below the buses: its UARTs and GPIOs are used as if they were part of a bus
  float get_setup_priority() const override { return setup_priority::BUS - 0.1F; }

  // low level register access (the timings required by the chip are handled here)
  uint8_t read_reg(uint8_t reg);
  void write_reg(uint8_t reg, uint8_t value);
  void read_burst(uint8_t reg, uint8_t *data, size_t length, uint32_t delay_us = CMD_DELAY_US);
  void write_burst(uint8_t reg, const uint8_t *data, size_t length);

  // GPIO helpers used by CH9438GPIOPin
  void gpio_enable(uint8_t pin);
  void gpio_set_mode(uint8_t pin, gpio::Flags flags);
  bool gpio_read(uint8_t pin);
  void gpio_write(uint8_t pin, bool value);

 protected:
  friend class CH9438Channel;
  friend class CH9438GPIOPin;

  /// @brief wait until the chip had its minimum idle time since the previous transaction
  void gap_();
  /// @brief write a multiplexing command and wait for the chip acknowledge
  bool iofunc_set_(uint8_t cmd, uint8_t addr, bool enable);
  bool probe_();
  bool config_pins_();
  bool config_clock_();
  void config_tnow_();
  void detect_tx_counter_semantic_();
  /// @brief read the status of all UARTs in one SPI transaction and move the received bytes to the ring buffers
  void poll_();

  std::vector<CH9438Channel *> children_{};
  InternalGPIOPin *irq_pin_{nullptr};
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *version_text_sensor_{nullptr};
#endif
  const char *name_{"ch9438"};
  uint32_t last_end_us_{0};
  uint8_t chip_ver_[4]{0, 0, 0, 0};
  uint8_t tnow_mask_{0};
  uint8_t tnow_invert_{0};
  uint8_t cts_mask_{0};  ///< bit n: CTS of UART n used (n = 1..4)
  uint8_t rts_mask_{0};  ///< bit n: RTS of UART n used (n = 1..4)
  uint8_t gpio_dir_{0};     ///< bit n = 1: GPIO n is an output
  uint8_t gpio_out_{0};     ///< last level written to each output
  uint8_t gpio_pull_{0};    ///< bit n = 1: GPIO n is an input with a pull resistor
  uint8_t gpio_pullup_{0};  ///< for the inputs with a resistor: 1 = pull-up, 0 = pull-down
  uint8_t gpio_enabled_{0};
  bool crystal_{false};
  /// true: the TX FIFO counter register returns the free space (datasheet); false: it returns the used space
  bool tx_counter_is_free_{true};
};

}  // namespace esphome::ch9438
