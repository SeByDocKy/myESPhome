/// @file ch9438.cpp
/// @brief WCH CH9438 (SPI to 8 UART + 8 GPIO) - classes implementation

#include "ch9438.h"
#include <cinttypes>
#include <cstdio>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ch9438 {

static const char *const TAG = "ch9438";

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
  this->gap_();
  this->enable();
  this->write_byte(reg & static_cast<uint8_t>(~CMD_WRITE_FLAG));
  delayMicroseconds(CMD_DELAY_US);
  uint8_t const value = this->read_byte();
  this->disable();
  this->last_end_us_ = micros();
  ESP_LOGVV(TAG, "read_reg(0x%02X) = 0x%02X", reg, value);
  return value;
}

void CH9438Component::write_reg(uint8_t reg, uint8_t value) {
  this->gap_();
  this->enable();
  this->write_byte(reg | CMD_WRITE_FLAG);
  delayMicroseconds(CMD_DELAY_US);
  this->write_byte(value);
  this->disable();
  this->last_end_us_ = micros();
  ESP_LOGVV(TAG, "write_reg(0x%02X) = 0x%02X", reg, value);
}

void CH9438Component::read_burst(uint8_t reg, uint8_t *data, size_t length, uint32_t delay_us) {
  this->gap_();
  this->enable();
  this->write_byte(reg & static_cast<uint8_t>(~CMD_WRITE_FLAG));
  delayMicroseconds(delay_us);
  this->read_array(data, length);
  this->disable();
  this->last_end_us_ = micros();
  ESP_LOGVV(TAG, "read_burst(0x%02X) %u bytes", reg, (unsigned) length);
}

void CH9438Component::write_burst(uint8_t reg, const uint8_t *data, size_t length) {
  this->gap_();
  this->enable();
  this->write_byte(reg | CMD_WRITE_FLAG);
  delayMicroseconds(CMD_DELAY_US);
  this->write_array(data, length);
  this->disable();
  this->last_end_us_ = micros();
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
    ESP_LOGW(TAG, "External crystal requested but clock register reads 0x%02X (is the 16 MHz crystal fitted?)", value);
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
  delay(10);  // let the chip finish its power-up

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
                "  Clock: %s\n"
                "  UARTs: %u\n"
                "  TNOW mask: 0x%02X (invert 0x%02X)",
                this->name_, this->chip_ver_[1] & 0x0F, this->chip_ver_[0], this->chip_ver_[0], this->chip_ver_[1],
                this->chip_ver_[2], this->chip_ver_[3], this->crystal_ ? "external 16 MHz crystal" : "internal",
                (unsigned) this->children_.size(), this->tnow_mask_, this->tnow_invert_);
  LOG_PIN("  CS Pin: ", this->cs_);
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
  if (this->irq_pin_ != nullptr && this->irq_pin_->digital_read())
    return;
  this->poll_();
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
    this->gpio_dir_ |= 1 << pin;
  } else {
    bool const pull = (flags & (gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN)) != 0;
    nibble = GPIO_DIR_INPUT | (pull ? GPIO_CFG_IN_PULL : GPIO_CFG_IN_FLOATING);
    this->gpio_dir_ &= ~(1 << pin);
    if (pull) {
      // for an input with a resistor, GPIO_PIN_VAL selects it: 1 = pull-up, 0 = pull-down. The register is also the
      // output level register, so it is always rewritten as a whole from the shadow values.
      this->gpio_pull_ |= 1 << pin;
      if (flags & gpio::FLAG_PULLUP)
        this->gpio_pullup_ |= 1 << pin;
      else
        this->gpio_pullup_ &= ~(1 << pin);
    } else {
      this->gpio_pull_ &= ~(1 << pin);
    }
  }
  uint8_t const reg = REG_GPIO_DIR_MOD0 + pin / 2;
  uint8_t value = this->read_reg(reg);
  if (pin & 1)
    value = static_cast<uint8_t>((value & 0x0F) | (nibble << 4));
  else
    value = static_cast<uint8_t>((value & 0xF0) | nibble);
  this->write_reg(reg, value);
  if (this->gpio_pull_ & (1 << pin))
    this->write_reg(REG_GPIO_PIN_VAL, static_cast<uint8_t>((this->gpio_pullup_ & this->gpio_pull_) |
                                                           (this->gpio_out_ & this->gpio_dir_)));
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
  if (value) {
    this->gpio_out_ |= 1 << pin;
    this->write_reg(REG_GPIO_SET, 1 << pin);
  } else {
    this->gpio_out_ &= ~(1 << pin);
    this->write_reg(REG_GPIO_RESET, 1 << pin);
  }
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
  if (baud < BAUD_MIN || baud > BAUD_MAX) {
    uint32_t const wanted = baud;
    baud = std::min(std::max(baud, BAUD_MIN), BAUD_MAX);
    ESP_LOGE(TAG, "UART%u: baud rate %" PRIu32 " is out of range, using %" PRIu32, this->channel_, wanted, baud);
  }
  // baud = 192 MHz / divisor, the divisor is a 16 bit value in 12.4 fixed point format (DLM:DLL)
  uint32_t divisor = (UART_REF_CLOCK + baud / 2) / baud;
  divisor = std::min<uint32_t>(std::max<uint32_t>(divisor, 16), 0xFFFF);
  auto *chip = this->parent_;
  chip->write_reg(this->reg_addr_(UART_LCR), this->lcr_ | LCR_DLAB);
  chip->write_reg(this->reg_addr_(UART_DLL), divisor & 0xFF);
  chip->write_reg(this->reg_addr_(UART_DLM), (divisor >> 8) & 0xFF);
  chip->write_reg(this->reg_addr_(UART_LCR), this->lcr_);
  ESP_LOGV(TAG, "UART%u baud rate %" PRIu32 " => divisor 0x%04" PRIX32 " (actual %" PRIu32 " Bd)", this->channel_, baud,
           divisor, UART_REF_CLOCK / divisor);
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

}  // namespace esphome::ch9438
