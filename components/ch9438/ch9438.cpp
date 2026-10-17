/// @file ch9438.cpp
/// @brief WCH CH9438 (SPI to 8 UART + 8 GPIO) - classes implementation

#include "ch9438.h"
#include <cinttypes>
#include <cstdio>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ch9438 {

static const char *const TAG = "ch9438";

// SPI statistics, only compiled in when the test mode is used (test_mode option)
#ifdef CH9438_TEST_COMPONENT
#define SPI_STAT_BEGIN() uint32_t const spi_t0 = micros()
#define SPI_STAT_END() \
  do { \
    this->spi_count_++; \
    this->spi_us_ += micros() - spi_t0; \
  } while (0)
#else
#define SPI_STAT_BEGIN() \
  do { \
  } while (0)
#define SPI_STAT_END() \
  do { \
  } while (0)
#endif

static const char *parity_name(uart::UARTParityOptions parity) {
  switch (parity) {
    case uart::UART_CONFIG_PARITY_NONE:
      return "NONE";
    case uart::UART_CONFIG_PARITY_EVEN:
      return "EVEN";
    case uart::UART_CONFIG_PARITY_ODD:
      return "ODD";
    default:
      return "UNKNOWN";
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// CH9438Component: SPI transport
// ---------------------------------------------------------------------------------------------------------------------
void CH9438Component::gap_() {
  // the chip needs a few microseconds between the release of CS and the next transaction
  while (static_cast<uint32_t>(micros() - this->last_end_us_) < GAP_US) {
  }
}

uint8_t CH9438Component::read_reg(uint8_t reg) {
  SPI_STAT_BEGIN();
  this->gap_();
  this->enable();
  this->write_byte(reg & static_cast<uint8_t>(~CMD_WRITE_FLAG));
  delayMicroseconds(CMD_DELAY_US);
  uint8_t const value = this->read_byte();
  this->disable();
  this->last_end_us_ = micros();
  SPI_STAT_END();
  ESP_LOGVV(TAG, "read_reg(0x%02X) = 0x%02X", reg, value);
  return value;
}

void CH9438Component::write_reg(uint8_t reg, uint8_t value) {
  SPI_STAT_BEGIN();
  this->gap_();
  this->enable();
  this->write_byte(reg | CMD_WRITE_FLAG);
  delayMicroseconds(CMD_DELAY_US);
  this->write_byte(value);
  this->disable();
  this->last_end_us_ = micros();
  SPI_STAT_END();
  ESP_LOGVV(TAG, "write_reg(0x%02X) = 0x%02X", reg, value);
}

void CH9438Component::read_burst(uint8_t reg, uint8_t *data, size_t length, uint32_t delay_us) {
  SPI_STAT_BEGIN();
  this->gap_();
  this->enable();
  this->write_byte(reg & static_cast<uint8_t>(~CMD_WRITE_FLAG));
  delayMicroseconds(delay_us);
  this->read_array(data, length);
  this->disable();
  this->last_end_us_ = micros();
  SPI_STAT_END();
  ESP_LOGVV(TAG, "read_burst(0x%02X) %u bytes", reg, (unsigned) length);
}

void CH9438Component::write_burst(uint8_t reg, const uint8_t *data, size_t length) {
  SPI_STAT_BEGIN();
  this->gap_();
  this->enable();
  this->write_byte(reg | CMD_WRITE_FLAG);
  delayMicroseconds(CMD_DELAY_US);
  this->write_array(data, length);
  this->disable();
  this->last_end_us_ = micros();
  SPI_STAT_END();
  ESP_LOGVV(TAG, "write_burst(0x%02X) %u bytes", reg, (unsigned) length);
}

// ---------------------------------------------------------------------------------------------------------------------
// CH9438Component: initialisation
// ---------------------------------------------------------------------------------------------------------------------
bool CH9438Component::iofunc_set_(uint8_t cmd, uint8_t addr, bool enable) {
  // the chip requires the complete 4 byte access: [command][pin address][enable][0xA5], then it answers with 0x5A
  uint8_t const out[4] = {cmd, addr, static_cast<uint8_t>(enable ? 1 : 0), IO_ACT};
  this->write_burst(REG_IO_SEL_FUN_CFG, out, sizeof(out));
  delayMicroseconds(1000);
  for (uint8_t attempt = 0; attempt < 20; attempt++) {
    uint8_t in[4] = {0, 0, 0, 0};
    this->read_burst(REG_IO_SEL_FUN_CFG, in, sizeof(in));
    if (in[3] == IO_DONE && in[0] == cmd && in[1] == addr)
      return true;
    delayMicroseconds(1000);
  }
  ESP_LOGE(TAG, "Pin function command 0x%02X addr %u not acknowledged", cmd, addr);
  return false;
}

bool CH9438Component::probe_() {
  // 1. scratch register test: a write/read of 0x55 and 0xAA on UART0 and on each configured UART
  std::vector<uint8_t> test_channels{0};
  for (auto *child : this->children_) {
    if (child->get_channel() != 0)
      test_channels.push_back(child->get_channel());
  }
  for (uint8_t const ch : test_channels) {
    for (uint8_t const pattern : {static_cast<uint8_t>(0x55), static_cast<uint8_t>(0xAA)}) {
      uint8_t const addr = static_cast<uint8_t>(ch * 8 + UART_SPR);
      this->write_reg(addr, pattern);
      uint8_t const value = this->read_reg(addr);
      if (value != pattern) {
        ESP_LOGE(TAG, "SPI test failed on UART%u: wrote 0x%02X, read 0x%02X (check wiring, SPI mode and CS)", ch,
                 pattern, value);
        return false;
      }
    }
  }
  // 2. chip identification: VER[2] = VER[0] + VER[1] and VER[3] = 0x7C
  this->read_burst(REG_CHIP_VER, this->chip_ver_, sizeof(this->chip_ver_));
  if (this->chip_ver_[3] != CHIP_VER3_ID ||
      this->chip_ver_[2] != static_cast<uint8_t>(this->chip_ver_[0] + this->chip_ver_[1])) {
    ESP_LOGE(TAG, "Not a CH9438: version bytes %02X %02X %02X %02X (expected VER[2]=VER[0]+VER[1], VER[3]=0x7C)",
             this->chip_ver_[0], this->chip_ver_[1], this->chip_ver_[2], this->chip_ver_[3]);
    return false;
  }
  return true;
}

bool CH9438Component::config_pins_() {
  bool ok = true;
  // 1. switch off every multiplexed function that is not used (the chip may keep a previous configuration after a
  //    reboot of the microcontroller). This is done first because several functions share the same physical pin.
  for (uint8_t n = 0; n < NUM_CHANNELS; n++) {
    if (!(this->tnow_mask_ & (1 << n)))
      ok &= this->iofunc_set_(IO_CMD_MULTI_SET, IO_ADDR_TNOW0 + n, false);
  }
  for (uint8_t c = 1; c <= 4; c++) {
    if (!(this->cts_mask_ & (1 << c)))
      ok &= this->iofunc_set_(IO_CMD_MULTI_SET, IO_ADDR_CTS1 + c - 1, false);
    if (!(this->rts_mask_ & (1 << c)))
      ok &= this->iofunc_set_(IO_CMD_MULTI_SET, IO_ADDR_RTS1 + c - 1, false);
  }
  for (uint8_t n = 0; n < NUM_GPIO; n++)
    ok &= this->iofunc_set_(IO_CMD_MULTI_SET, IO_ADDR_GPIO0 + n, false);  // enabled later by the GPIO pins
  if (this->irq_pin_ == nullptr)
    ok &= this->iofunc_set_(IO_CMD_MULTI_SET, IO_ADDR_INT, false);

  // 2. switch on what is used
  if (this->irq_pin_ != nullptr)
    ok &= this->iofunc_set_(IO_CMD_MULTI_SET, IO_ADDR_INT, true);
  for (auto *child : this->children_)
    ok &= this->iofunc_set_(IO_CMD_DEFAULT_SET, IO_ADDR_UART0 + child->get_channel(), true);
  ok &= this->iofunc_set_(IO_CMD_DEFAULT_SET, IO_ADDR_HSE, this->crystal_);
  for (uint8_t n = 0; n < NUM_CHANNELS; n++) {
    if (this->tnow_mask_ & (1 << n))
      ok &= this->iofunc_set_(IO_CMD_MULTI_SET, IO_ADDR_TNOW0 + n, true);
  }
  for (uint8_t c = 1; c <= 4; c++) {
    if (this->cts_mask_ & (1 << c))
      ok &= this->iofunc_set_(IO_CMD_MULTI_SET, IO_ADDR_CTS1 + c - 1, true);
    if (this->rts_mask_ & (1 << c))
      ok &= this->iofunc_set_(IO_CMD_MULTI_SET, IO_ADDR_RTS1 + c - 1, true);
  }
  return ok;
}

bool CH9438Component::config_clock_() {
  this->write_reg(REG_CLK_CTRL, this->crystal_ ? CLK_EXTERNAL : CLK_INTERNAL);
  delay(CLK_SETTLE_MS);
  uint8_t const value = this->read_reg(REG_CLK_CTRL);
  ESP_LOGV(TAG, "Clock register = 0x%02X", value);
  if (this->crystal_ && (value & CLK_EXTERNAL) != CLK_EXTERNAL) {
    ESP_LOGW(TAG, "External crystal requested but clock register reads 0x%02X (is the crystal fitted?)", value);
    return false;
  }
  return true;
}

void CH9438Component::config_tnow_() {
  this->write_reg(REG_TNOW_CTRL1, static_cast<uint8_t>((this->tnow_mask_ & 0x0F) | ((this->tnow_invert_ & 0x0F) << 4)));
  this->write_reg(REG_TNOW_CTRL2,
                  static_cast<uint8_t>(((this->tnow_mask_ >> 4) & 0x0F) | (((this->tnow_invert_ >> 4) & 0x0F) << 4)));
}

void CH9438Component::detect_tx_counter_semantic_() {
  if (this->children_.empty())
    return;
  // The datasheet says that the TX FIFO counter gives the remaining room. The FIFO of an idle UART has just been
  // reset, so the counter reads close to 1536 if it is a free-space counter and 0 if it counts the used bytes.
  uint8_t const ch = this->children_[0]->get_channel();
  this->write_reg(REG_FIFO_CTRL, FIFO_DIR_TX | ch);
  uint8_t const lo = this->read_reg(REG_FIFO_CNT_L);
  uint8_t const hi = this->read_reg(REG_FIFO_CNT_H);
  size_t const count = lo | (hi << 8);
  if (count == 0) {
    this->tx_counter_is_free_ = false;
    ESP_LOGCONFIG(TAG, "TX FIFO counter of an idle UART reads 0: it is treated as the number of bytes in the FIFO");
  } else {
    this->tx_counter_is_free_ = true;
    if (count < TX_FIFO_SIZE - 64) {
      ESP_LOGW(TAG, "TX FIFO counter of an idle UART reads %u (expected ~%u): treated as free space", (unsigned) count,
               (unsigned) TX_FIFO_SIZE);
    } else {
      ESP_LOGV(TAG, "TX FIFO counter of an idle UART reads %u: treated as free space", (unsigned) count);
    }
  }
}

void CH9438Component::setup() {
  this->spi_setup();
  if (this->irq_pin_ != nullptr)
    this->irq_pin_->setup();
  // The vendor reference code resets the chip (RST# low 100 ms, then high 100 ms) and waits 500 ms before the first
  // access. Without a reset pin we only make sure that 500 ms have elapsed since the boot of the microcontroller (the
  // chip is normally powered together with it).
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->reset_pin_->digital_write(false);
    delay(RESET_PULSE_MS);
    this->reset_pin_->digital_write(true);
    delay(RESET_PULSE_MS);
  }
  uint32_t const uptime_ms = millis();
  if (uptime_ms < STARTUP_WAIT_MS)
    delay(STARTUP_WAIT_MS - uptime_ms);

  bool probed = false;
  for (uint8_t attempt = 0; attempt < SPI_PROBE_RETRIES && !probed; attempt++) {
    probed = this->probe_();
    if (!probed)
      delay(20);
  }
  if (!probed) {
    this->mark_failed();
    return;
  }

  bool ok = this->config_pins_();
  ok &= this->config_clock_();
  this->config_tnow_();
  for (auto *child : this->children_)
    child->setup_channel();
  this->detect_tx_counter_semantic_();
  if (!ok)
    this->status_set_warning();

#ifdef USE_TEXT_SENSOR
  if (this->version_text_sensor_ != nullptr) {
    char version[16];
    snprintf(version, sizeof(version), "V%X.%02X", this->chip_ver_[1] & 0x0F, this->chip_ver_[0]);
    this->version_text_sensor_->publish_state(version);
  }
#endif
}

void CH9438Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "CH9438 %s:\n"
                "  Version: V%X.%02X (raw %02X %02X %02X %02X)\n"
                "  Clock: %s (%" PRIu32 " Hz)\n"
                "  UART reference clock: %" PRIu32 " Hz (baud rate range %" PRIu32 " to %" PRIu32 " Bd)\n"
                "  UARTs: %u\n"
                "  TNOW mask: 0x%02X (invert 0x%02X)",
                this->name_, this->chip_ver_[1] & 0x0F, this->chip_ver_[0], this->chip_ver_[0], this->chip_ver_[1],
                this->chip_ver_[2], this->chip_ver_[3], this->crystal_ ? "external crystal" : "internal",
                this->crystal_hz_, this->uart_clock_, this->get_baud_min(), this->get_baud_max(),
                (unsigned) this->children_.size(), this->tnow_mask_, this->tnow_invert_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  if (this->irq_pin_ != nullptr) {
    LOG_PIN("  IRQ Pin: ", this->irq_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  IRQ Pin: not used (the status of the chip is polled at every loop)");
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication with the chip failed!");
  }
  for (auto *child : this->children_)
    child->dump_channel();
}

// ---------------------------------------------------------------------------------------------------------------------
// CH9438Component: reception
// ---------------------------------------------------------------------------------------------------------------------
void CH9438Component::loop() {
  if (this->children_.empty())
    return;
  // INT# is active low: when it is high nothing is pending and the bus is left alone
  bool const pending = this->irq_pin_ == nullptr || !this->irq_pin_->digital_read();
#ifdef CH9438_TEST_COMPONENT
  this->test_pre_loop_();
  uint32_t const poll_start_us = micros();
#endif
  if (pending)
    this->poll_();
#ifdef CH9438_TEST_COMPONENT
  if (pending)
    this->test_poll_done_(micros() - poll_start_us);
  this->test_run_();
#endif
}

void CH9438Component::poll_() {
  // One transaction returns, for each of the 8 UARTs: IIR, LSR, MSR, RX count (L, H), TX count (L, H)
  uint8_t status[BULK_SIZE];
  this->read_burst(REG_BULK, status, sizeof(status), BULK_DELAY_US);
  for (auto *child : this->children_) {
    const uint8_t *r = status + BULK_BYTES_PER_UART * child->get_channel();
    uint8_t const lsr = r[1];
    if (lsr & LSR_ERROR_MASK) {
      ESP_LOGW(TAG, "UART%u line status error LSR=0x%02X%s%s%s%s", child->get_channel(), lsr,
               (lsr & LSR_OE) ? " overrun" : "", (lsr & LSR_PE) ? " parity" : "", (lsr & LSR_FE) ? " framing" : "",
               (lsr & LSR_BI) ? " break" : "");
    }
    size_t rx_count = static_cast<size_t>(r[3]) | (static_cast<size_t>(r[4]) << 8);
    if (rx_count == 0 || rx_count == 0xFFFF)  // 0xFFFF is not a valid count
      continue;
    rx_count = std::min(rx_count, RX_FIFO_SIZE);
    size_t const moved = child->xfer_fifo_to_buffer_(rx_count);
#ifdef CH9438_TEST_COMPONENT
    this->test_rx_bytes_ += moved;
#endif
    if (moved > 0) {
      ESP_LOGV(TAG, "UART%u: %u of %u bytes moved to the ring buffer", child->get_channel(), (unsigned) moved,
               (unsigned) rx_count);
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// CH9438Component: GPIO
// ---------------------------------------------------------------------------------------------------------------------
void CH9438Component::gpio_enable(uint8_t pin) {
  if (pin >= NUM_GPIO || (this->gpio_enabled_ & (1 << pin)))
    return;
  // the Python validation guarantees that the physical pin is not used by another function
  if (!this->iofunc_set_(IO_CMD_MULTI_SET, IO_ADDR_GPIO0 + pin, true))
    return;
  this->gpio_enabled_ |= 1 << pin;
  this->write_reg(REG_GPIO_FUNC_EN, this->gpio_enabled_);
}

void CH9438Component::gpio_set_mode(uint8_t pin, gpio::Flags flags) {
  if (pin >= NUM_GPIO)
    return;
  uint8_t nibble;
  if (flags & gpio::FLAG_OUTPUT) {
    nibble = GPIO_DIR_OUTPUT | ((flags & gpio::FLAG_OPEN_DRAIN) ? GPIO_CFG_OUT_OPENDRAIN : GPIO_CFG_OUT_PUSHPULL);
  } else {
    bool const pull = (flags & (gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN)) != 0;
    nibble = GPIO_DIR_INPUT | (pull ? GPIO_CFG_IN_PULL : GPIO_CFG_IN_FLOATING);
  }
  // for an input with a resistor the vendor reference code selects pull-up / pull-down by writing the pin to the SET
  // (pull-up) or RESET (pull-down) register before programming the mode
  if (!(flags & gpio::FLAG_OUTPUT) && (flags & (gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN)))
    this->write_reg((flags & gpio::FLAG_PULLUP) ? REG_GPIO_SET : REG_GPIO_RESET, 1 << pin);
  uint8_t const reg = REG_GPIO_DIR_MOD0 + pin / 2;
  uint8_t value = this->read_reg(reg);
  if (pin & 1)
    value = static_cast<uint8_t>((value & 0x0F) | (nibble << 4));
  else
    value = static_cast<uint8_t>((value & 0xF0) | nibble);
  this->write_reg(reg, value);
}

bool CH9438Component::gpio_read(uint8_t pin) {
  if (pin >= NUM_GPIO)
    return false;
  return (this->read_reg(REG_GPIO_PIN_VAL) >> pin) & 1;
}

void CH9438Component::gpio_write(uint8_t pin, bool value) {
  if (pin >= NUM_GPIO)
    return;
  // SET / RESET only act on the bits written to 1, so the other outputs are not disturbed
  this->write_reg(value ? REG_GPIO_SET : REG_GPIO_RESET, 1 << pin);
}

void CH9438GPIOPin::setup() {
  this->parent_->gpio_enable(this->pin_);
  this->pin_mode(this->flags_);
}

void CH9438GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->gpio_set_mode(this->pin_, flags); }

bool CH9438GPIOPin::digital_read() { return this->parent_->gpio_read(this->pin_) != this->inverted_; }

void CH9438GPIOPin::digital_write(bool value) { this->parent_->gpio_write(this->pin_, value != this->inverted_); }

size_t CH9438GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via CH9438 %s", this->pin_, this->parent_->get_name());
}

// ---------------------------------------------------------------------------------------------------------------------
// CH9438Channel
// ---------------------------------------------------------------------------------------------------------------------
void CH9438Channel::set_parent(CH9438Component *parent) {
  this->parent_ = parent;
  parent->children_.push_back(this);
}

void CH9438Channel::setup_channel() {
  auto *chip = this->parent_;
  chip->write_reg(this->reg_addr_(UART_IER), 0);  // no interrupt while the UART is being set up
  chip->write_reg(this->reg_addr_(UART_MCR), 0);
  chip->write_reg(this->reg_addr_(UART_FCR), 0);
  // reset both FIFOs, then enable them (without FIFOEN the UART is closed)
  chip->write_reg(this->reg_addr_(UART_FCR), FCR_RXRST | FCR_TXRST);
  delay(FCR_SETTLE_MS);
  chip->write_reg(this->reg_addr_(UART_FCR), FCR_FIFOEN);
  this->receive_buffer_.init(this->rx_buffer_size_);
  this->set_line_param_();
  this->set_baudrate_();
  chip->write_reg(this->reg_addr_(UART_MCR), this->flow_control_ ? (MCR_AFE | MCR_RTS) : 0);
  // receive data and line status interrupts only: the transmit side is polled and the modem lines are handled by the
  // automatic flow control of the chip
  chip->write_reg(this->reg_addr_(UART_IER), IER_RDI | IER_RLSI);
}

bool CH9438Channel::change_baud_rate(uint32_t baud_rate) {
  uint32_t const baud_min = this->parent_->get_baud_min();
  uint32_t const baud_max = this->parent_->get_baud_max();
  if (baud_rate < baud_min || baud_rate > baud_max) {
    ESP_LOGE(TAG, "UART%u: cannot change the baud rate to %" PRIu32 " Bd (supported range %" PRIu32 " to %" PRIu32 ")",
             this->channel_, baud_rate, baud_min, baud_max);
    return false;
  }
  if (baud_rate == this->baud_rate_)
    return true;
  // the bytes already queued must leave at the old speed
  this->flush();
  uint32_t const old_baud = this->baud_rate_;
  this->baud_rate_ = baud_rate;
  this->set_baudrate_();
  ESP_LOGI(TAG, "UART%u: baud rate changed from %" PRIu32 " to %" PRIu32 " Bd", this->channel_, old_baud, baud_rate);
  return true;
}

void CH9438Channel::dump_channel() {
  ESP_LOGCONFIG(TAG,
                "  UART%u (%s):\n"
                "    Baud rate: %" PRIu32 " Bd\n"
                "    Data bits: %u\n"
                "    Stop bits: %u\n"
                "    Parity: %s\n"
                "    Flow control: %s\n"
                "    RX ring buffer: %u bytes",
                this->channel_, this->name_, this->baud_rate_, this->data_bits_, this->stop_bits_,
                parity_name(this->parity_), this->flow_control_ ? "RTS/CTS" : "none",
                (unsigned) this->rx_buffer_size_);
}

void CH9438Channel::set_line_param_() {
  this->data_bits_ = 8;  // the chip only supports 8 data bits
  uint8_t lcr = LCR_WORD_8;
  if (this->stop_bits_ == 2)
    lcr |= LCR_STOP2;
  switch (this->parity_) {
    case uart::UART_CONFIG_PARITY_ODD:
      lcr |= LCR_PAREN;
      break;
    case uart::UART_CONFIG_PARITY_EVEN:
      lcr |= LCR_PAREN | LCR_PAR_EVEN;
      break;
    default:
      break;
  }
  this->lcr_ = lcr;
  this->parent_->write_reg(this->reg_addr_(UART_LCR), lcr);
  ESP_LOGV(TAG, "UART%u LCR = 0x%02X", this->channel_, lcr);
}

void CH9438Channel::set_baudrate_() {
  uint32_t baud = this->baud_rate_;
  uint32_t const uart_clock = this->parent_->get_uart_clock();
  uint32_t const baud_min = this->parent_->get_baud_min();
  uint32_t const baud_max = this->parent_->get_baud_max();
  if (baud < baud_min || baud > baud_max) {
    uint32_t const wanted = baud;
    baud = std::min(std::max(baud, baud_min), baud_max);
    ESP_LOGE(TAG, "UART%u: baud rate %" PRIu32 " is out of range, using %" PRIu32, this->channel_, wanted, baud);
  }
  // baud = uart_clock / divisor (192 MHz, or 12 x crystal), the divisor is a 16 bit value in 12.4 fixed point (DLM:DLL)
  uint32_t divisor = (uart_clock + baud / 2) / baud;
  divisor = std::min<uint32_t>(std::max<uint32_t>(divisor, DIVISOR_MIN), DIVISOR_MAX);
  auto *chip = this->parent_;
  chip->write_reg(this->reg_addr_(UART_LCR), this->lcr_ | LCR_DLAB);
  chip->write_reg(this->reg_addr_(UART_DLL), divisor & 0xFF);
  chip->write_reg(this->reg_addr_(UART_DLM), (divisor >> 8) & 0xFF);
  chip->write_reg(this->reg_addr_(UART_LCR), this->lcr_);
  ESP_LOGV(TAG, "UART%u baud rate %" PRIu32 " => divisor 0x%04" PRIX32 " (actual %" PRIu32 " Bd)", this->channel_, baud,
           divisor, uart_clock / divisor);
}

size_t CH9438Channel::tx_free_() {
  auto *chip = this->parent_;
  chip->write_reg(REG_FIFO_CTRL, FIFO_DIR_TX | this->channel_);
  uint8_t const lo = chip->read_reg(REG_FIFO_CNT_L);
  uint8_t const hi = chip->read_reg(REG_FIFO_CNT_H);
  size_t const count = std::min<size_t>(lo | (hi << 8), TX_FIFO_SIZE);
  return chip->tx_counter_is_free_ ? count : TX_FIFO_SIZE - count;
}

size_t CH9438Channel::xfer_fifo_to_buffer_(size_t count) {
  size_t total = 0;
  while (count > 0) {
    size_t const chunk = std::min({count, this->receive_buffer_.contiguous_free(), XFER_MAX_SIZE});
    if (chunk == 0) {
      // the ring buffer is full: the bytes stay in the chip FIFO (2048 bytes) until the consumer has read some
      if (millis() - this->last_overflow_log_ > 1000) {
        this->last_overflow_log_ = millis();
        ESP_LOGW(TAG, "UART%u: receive ring buffer full, %u bytes left in the chip FIFO", this->channel_,
                 (unsigned) count);
      }
      break;
    }
    // the FIFO is read directly into the ring buffer
    this->parent_->read_burst(this->reg_addr_(UART_RBR), this->receive_buffer_.write_ptr(), chunk);
    this->receive_buffer_.commit(chunk);
    count -= chunk;
    total += chunk;
  }
  return total;
}

size_t CH9438Channel::available() { return this->receive_buffer_.count(); }

bool CH9438Channel::peek_byte(uint8_t *buffer) { return this->receive_buffer_.peek(*buffer); }

bool CH9438Channel::read_array(uint8_t *buffer, size_t length) {
  bool status = true;
  size_t const available = this->receive_buffer_.count();
  if (length > available) {
    ESP_LOGW(TAG, "UART%u read_array: %u bytes requested but only %u available", this->channel_, (unsigned) length,
             (unsigned) available);
    length = available;
    status = false;
  }
  this->receive_buffer_.pop_bulk(buffer, length);
  return status;
}

void CH9438Channel::write_array(const uint8_t *buffer, size_t length) {
  uint32_t const start_time = millis();
  while (length > 0) {
    size_t const room = this->tx_free_();
    if (room == 0) {
      // transmit FIFO (1536 bytes) full: wait until the line has sent some bytes
      uint32_t const timeout_ms = 200 + static_cast<uint32_t>((TX_FIFO_SIZE * 11000ULL) / std::max<uint32_t>(this->baud_rate_, 1));
      if (millis() - start_time > timeout_ms) {
        ESP_LOGE(TAG, "UART%u write_array: timeout waiting for room in the transmit FIFO - %u bytes dropped",
                 this->channel_, (unsigned) length);
        return;
      }
      yield();
      continue;
    }
    size_t const chunk = std::min({length, room, XFER_MAX_SIZE});
    this->parent_->write_burst(this->reg_addr_(UART_THR), buffer, chunk);
    buffer += chunk;
    length -= chunk;
  }
}

uart::UARTFlushResult CH9438Channel::flush() {
  uint32_t const timeout_ms = 100 + static_cast<uint32_t>((TX_FIFO_SIZE * 11000ULL) / std::max<uint32_t>(this->baud_rate_, 1));
  uint32_t const start_time = millis();
  while (true) {
    uint8_t const lsr = this->parent_->read_reg(this->reg_addr_(UART_LSR));
    if (lsr & LSR_TEMT)  // FIFO and shift register are empty
      return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;
    if (millis() - start_time > timeout_ms) {
      ESP_LOGW(TAG, "UART%u flush timeout after %" PRIu32 " ms", this->channel_, timeout_ms);
      return uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
    }
    yield();
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// TEST COMPONENT (compiled in only when the `test_mode` option is not 0)
// ---------------------------------------------------------------------------------------------------------------------
#ifdef CH9438_TEST_COMPONENT
static constexpr uint32_t TEST_STATS_PERIOD_MS = 10000;
static constexpr size_t TEST_PATTERN_SIZE = 1024;

void CH9438Component::test_pre_loop_() {
  uint32_t const now_us = micros();
  if (this->test_last_loop_us_ != 0)
    this->test_max_gap_us_ = std::max<uint32_t>(this->test_max_gap_us_, now_us - this->test_last_loop_us_);
  this->test_last_loop_us_ = now_us;
  this->test_loops_++;
}

void CH9438Component::test_poll_done_(uint32_t duration_us) {
  this->test_polls_++;
  this->test_poll_us_ += duration_us;
  this->test_poll_max_us_ = std::max(this->test_poll_max_us_, duration_us);
}

void CH9438Component::test_report_stats_(uint32_t now) {
  uint32_t const elapsed_ms = now - this->test_stats_ms_;
  if (elapsed_ms < TEST_STATS_PERIOD_MS)
    return;
  uint64_t const e = std::max<uint32_t>(elapsed_ms, 1);
  // everything is computed with integers (per mille for the percentages)
  uint32_t const busy_pm = static_cast<uint32_t>((static_cast<uint64_t>(this->spi_us_) * 1000ULL) / (e * 1000ULL));
  ESP_LOGI(TAG,
           "[stats %" PRIu32 " ms] loop: %" PRIu32 " calls (%" PRIu32 "/s), longest gap between two calls %" PRIu32
           " us | status reads: %" PRIu32 ", mean %" PRIu32 " us, max %" PRIu32 " us | SPI: %" PRIu32
           " transactions (%" PRIu32 "/s), busy %" PRIu32 ".%" PRIu32 " %% of the time | %" PRIu32
           " bytes moved to the ring buffers (%" PRIu32 " B/s)",
           elapsed_ms, this->test_loops_, static_cast<uint32_t>(this->test_loops_ * 1000ULL / e), this->test_max_gap_us_,
           this->test_polls_, this->test_polls_ ? this->test_poll_us_ / this->test_polls_ : 0, this->test_poll_max_us_,
           this->spi_count_, static_cast<uint32_t>(this->spi_count_ * 1000ULL / e), busy_pm / 10, busy_pm % 10,
           this->test_rx_bytes_, static_cast<uint32_t>(this->test_rx_bytes_ * 1000ULL / e));
  this->spi_count_ = this->spi_us_ = 0;
  this->test_loops_ = this->test_polls_ = this->test_poll_us_ = this->test_poll_max_us_ = this->test_max_gap_us_ = 0;
  this->test_rx_bytes_ = 0;
  this->test_stats_ms_ = now;
}

/// Loopback test: TX and RX of the tested UART must be wired together. The channels are tested one after the other
/// without ever blocking loop(). Mode 1 sends a short text, mode 2 a 1024 byte pattern and measures the throughput.
void CH9438Component::test_run_() {
  uint32_t const now = millis();
  if (this->test_stats_ms_ == 0)
    this->test_stats_ms_ = now;
  this->test_report_stats_(now);
  if (this->test_mode_ == 0 || this->children_.empty())
    return;

  if (this->test_idx_ >= this->children_.size())
    this->test_idx_ = 0;
  CH9438Channel *ch = this->children_[this->test_idx_];

  if (!this->test_busy_) {
    if (static_cast<int32_t>(now - this->test_next_ms_) < 0)
      return;
    this->test_tx_.clear();
    if (this->test_mode_ == 1) {
      char msg[48];
      int const len = snprintf(msg, sizeof(msg), "%s:%s:UART%u\n", this->name_, ch->get_channel_name(), ch->get_channel());
      this->test_tx_.assign(msg, msg + std::min<int>(len, sizeof(msg) - 1));
    } else {
      this->test_tx_.resize(TEST_PATTERN_SIZE);
      for (size_t i = 0; i < TEST_PATTERN_SIZE; i++)
        this->test_tx_[i] = static_cast<uint8_t>(i % 251);  // 251 is prime: a shifted stream is always detected
    }
    this->test_pos_ = 0;
    this->test_errors_ = 0;
    // 10 bits per byte on the line (start + 8 data + stop), twice the nominal time plus a margin
    uint32_t const line_ms = static_cast<uint32_t>(this->test_tx_.size() * 10000ULL / std::max<uint32_t>(ch->baud_rate_, 1));
    this->test_deadline_ms_ = now + 2 * line_ms + 500;
    this->test_start_us_ = micros();
    ch->write_array(this->test_tx_.data(), this->test_tx_.size());
    this->test_busy_ = true;
    return;
  }

  // reception: the ring buffer is filled by poll_(), here it is only emptied and compared with what was sent
  uint8_t buf[64];
  while (true) {
    size_t n = std::min(ch->available(), sizeof(buf));
    if (n == 0)
      break;
    ch->read_array(buf, n);
    for (size_t i = 0; i < n; i++) {
      if (this->test_pos_ < this->test_tx_.size() && buf[i] != this->test_tx_[this->test_pos_])
        this->test_errors_++;
      this->test_pos_++;
    }
  }
  bool const complete = this->test_pos_ >= this->test_tx_.size();
  bool const timeout = static_cast<int32_t>(now - this->test_deadline_ms_) > 0;
  if (!complete && !timeout)
    return;

  uint32_t const elapsed_us = std::max<uint32_t>(micros() - this->test_start_us_, 1);
  size_t const sent = this->test_tx_.size();
  uint32_t const rate = static_cast<uint32_t>(this->test_pos_ * 1000000ULL / elapsed_us);  // bytes per second
  uint32_t const line_rate = ch->baud_rate_ / 10;                                           // bytes per second
  this->test_runs_++;
  if (complete && this->test_errors_ == 0 && this->test_pos_ == sent) {
    ESP_LOGI(TAG, "[test %s] UART%u @ %" PRIu32 " Bd: %u bytes OK in %" PRIu32 " us -> %" PRIu32 " B/s (line limit %" PRIu32
                  " B/s, %" PRIu32 " %%)",
             ch->get_channel_name(), ch->get_channel(), ch->baud_rate_, (unsigned) sent, elapsed_us, rate, line_rate,
             static_cast<uint32_t>(static_cast<uint64_t>(rate) * 100ULL / std::max<uint32_t>(line_rate, 1)));
  } else {
    this->test_failures_++;
    ESP_LOGE(TAG, "[test %s] UART%u @ %" PRIu32 " Bd: FAILED, %u of %u bytes received, %" PRIu32
                  " wrong, %s (failures %" PRIu32 " of %" PRIu32 " runs). Are TX and RX wired together?",
             ch->get_channel_name(), ch->get_channel(), ch->baud_rate_, (unsigned) this->test_pos_, (unsigned) sent,
             this->test_errors_, timeout ? "timeout" : "complete", this->test_failures_, this->test_runs_);
  }
  this->test_busy_ = false;
  this->test_idx_++;
  this->test_next_ms_ = millis() + (this->test_mode_ == 1 ? 2000 : 1000);
}
#endif  // CH9438_TEST_COMPONENT

}  // namespace esphome::ch9438
