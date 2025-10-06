#include "cmt2300a.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cmt2300a {

static const char *const TAG = "cmt2300a";

void CMT2300AComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CMT2300A with bit-banging SPI...");
  
  if (this->sclk_pin_ == nullptr || this->sdio_pin_ == nullptr || 
      this->cs_pin_ == nullptr || this->fcs_pin_ == nullptr) {
    ESP_LOGE(TAG, "Required pins not configured!");
    this->mark_failed();
    return;
  }
  
  // Configuration des pins
  this->sclk_pin_->setup();
  this->sclk_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->sclk_pin_->digital_write(false);
  
  this->sdio_pin_->setup();
  this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT);
  
  this->cs_pin_->setup();
  this->cs_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->cs_pin_->digital_write(true);
  
  this->fcs_pin_->setup();
  this->fcs_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->fcs_pin_->digital_write(true);
  
  if (this->gpio1_pin_ != nullptr) {
    this->gpio1_pin_->setup();
    this->gpio1_pin_->pin_mode(gpio::FLAG_INPUT);
  }
  
  ESP_LOGD(TAG, "Bit-banging SPI initialized - SCLK: %d, SDIO: %d", 
           this->sclk_pin_->get_pin(), this->sdio_pin_->get_pin());
  
  delay(10);
  
  if (!this->reset_chip_()) {
    ESP_LOGE(TAG, "Failed to reset chip");
    this->mark_failed();
    return;
  }
  
  delay(100);
  
  uint8_t chip_id = this->read_register_(0x7F);
  ESP_LOGD(TAG, "Chip ID: 0x%02X", chip_id);
  
  if (!this->enter_standby_mode()) {
    ESP_LOGE(TAG, "Failed to enter standby mode");
    this->mark_failed();
    return;
  }
  
  delay(10);
  
  this->configure_frequency_();
  this->configure_data_rate_();
  this->configure_packet_format_();
  this->configure_tx_power_();
  this->calibrate_();
  
  this->clear_interrupt_flags_(0xFF);
  this->clear_fifo_();
  this->write_register_(0x0B, 0x1F);
  
  if (!this->enter_rx_mode()) {
    ESP_LOGE(TAG, "Failed to enter RX mode");
    this->mark_failed();
    return;
  }
  
  this->chip_ready_ = true;
  ESP_LOGCONFIG(TAG, "CMT2300A setup complete");
}

void CMT2300AComponent::loop() {
  if (!this->chip_ready_) return;
  
  if (this->gpio1_pin_ != nullptr && this->gpio1_pin_->digital_read()) {
    this->handle_interrupt_();
  }
}

void CMT2300AComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CMT2300A (Bit-banging SPI):");
  LOG_PIN("  SCLK: ", this->sclk_pin_);
  LOG_PIN("  SDIO: ", this->sdio_pin_);
  LOG_PIN("  CS: ", this->cs_pin_);
  LOG_PIN("  FCS: ", this->fcs_pin_);
  ESP_LOGCONFIG(TAG, "  Frequency: %.3f MHz", this->frequency_ / 1e6);
  ESP_LOGCONFIG(TAG, "  Data Rate: %.1f kbps", this->data_rate_ / 1e3);
  ESP_LOGCONFIG(TAG, "  TX Power: %d dBm", this->tx_power_);
  ESP_LOGCONFIG(TAG, "  Stats - TX: %u, RX: %u, Errors: %u", 
                this->tx_count_, this->rx_count_, this->crc_error_count_);
}

// Bit-banging SPI : Écriture d'un byte
void CMT2300AComponent::spi_write_byte_(uint8_t data) {
  // SDIO en OUTPUT
  this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT);
  
  for (int i = 7; i >= 0; i--) {
    // Préparer le bit
    this->sdio_pin_->digital_write((data >> i) & 0x01);
    delayMicroseconds(5);  // Augmenté de 1 à 5
    
    // Clock HIGH
    this->sclk_pin_->digital_write(true);
    delayMicroseconds(5);  // Augmenté de 1 à 5
    
    // Clock LOW
    this->sclk_pin_->digital_write(false);
    delayMicroseconds(5);  // Augmenté de 1 à 5
  }
}

// Bit-banging SPI : Lecture d'un byte
uint8_t CMT2300AComponent::spi_read_byte_() {
  uint8_t data = 0;
  
  // SDIO en INPUT
  this->sdio_pin_->pin_mode(gpio::FLAG_INPUT);
  delayMicroseconds(5);  // Augmenté de 2 à 5
  
  for (int i = 7; i >= 0; i--) {
    // Clock HIGH
    this->sclk_pin_->digital_write(true);
    delayMicroseconds(5);  // Augmenté de 1 à 5
    
    // Lire le bit
    if (this->sdio_pin_->digital_read()) {
      data |= (1 << i);
    }
    
    // Clock LOW
    this->sclk_pin_->digital_write(false);
    delayMicroseconds(5);  // Augmenté de 1 à 5
  }
  
  return data;
}

void CMT2300AComponent::write_register_(uint8_t reg, uint8_t value) {
  this->cs_pin_->digital_write(false);
  delayMicroseconds(10);  // Augmenté de 2 à 10
  
  this->spi_write_byte_(reg & 0x7F);
  this->spi_write_byte_(value);
  
  delayMicroseconds(10);  // Augmenté de 2 à 10
  this->cs_pin_->digital_write(true);
  delayMicroseconds(10);  // Ajouté délai après CS
  
  ESP_LOGVV(TAG, "Write reg 0x%02X = 0x%02X", reg, value);
}

uint8_t CMT2300AComponent::read_register_(uint8_t reg) {
  this->cs_pin_->digital_write(false);
  delayMicroseconds(10);  // Augmenté de 2 à 10
  
  this->spi_write_byte_(0x80 | (reg & 0x7F));
  delayMicroseconds(10);  // Augmenté de 2 à 10
  uint8_t value = this->spi_read_byte_();
  
  delayMicroseconds(10);  // Augmenté de 2 à 10
  this->cs_pin_->digital_write(true);
  delayMicroseconds(10);  // Ajouté délai après CS
  
  ESP_LOGVV(TAG, "Read reg 0x%02X = 0x%02X", reg, value);
  return value;
}

void CMT2300AComponent::write_fifo_(const std::vector<uint8_t> &data) {
  if (data.empty() || data.size() > CMT2300A_FIFO_SIZE) return;
  
  this->fcs_pin_->digital_write(false);
  delayMicroseconds(2);
  
  this->spi_write_byte_(CMT2300A_FIFO_WR);
  this->spi_write_byte_(data.size());
  
  for (uint8_t byte : data) {
    this->spi_write_byte_(byte);
  }
  
  delayMicroseconds(2);
  this->fcs_pin_->digital_write(true);
}

std::vector<uint8_t> CMT2300AComponent::read_fifo_() {
  std::vector<uint8_t> data;
  
  uint8_t length = this->get_fifo_length_();
  if (length == 0 || length > CMT2300A_FIFO_SIZE) return data;
  
  this->fcs_pin_->digital_write(false);
  delayMicroseconds(2);
  
  this->spi_write_byte_(CMT2300A_FIFO_RD);
  delayMicroseconds(2);
  uint8_t actual_length = this->spi_read_byte_();
  
  if (actual_length > 0 && actual_length <= CMT2300A_FIFO_SIZE) {
    for (uint8_t i = 0; i < actual_length; i++) {
      data.push_back(this->spi_read_byte_());
    }
  }
  
  delayMicroseconds(2);
  this->fcs_pin_->digital_write(true);
  
  return data;
}

void CMT2300AComponent::clear_fifo_() {
  this->fcs_pin_->digital_write(false);
  delayMicroseconds(2);
  
  this->spi_write_byte_(CMT2300A_FIFO_CLR);
  
  delayMicroseconds(2);
  this->fcs_pin_->digital_write(true);
}

uint8_t CMT2300AComponent::get_fifo_length_() {
  return this->read_register_(CMT2300A_REG_FIFO_FLAG) & 0x7F;
}

bool CMT2300AComponent::send_packet(const std::vector<uint8_t> &data) {
  if (!this->chip_ready_ || data.empty()) return false;
  
  if (!this->enter_standby_mode()) return false;
  this->clear_fifo_();
  this->write_fifo_(data);
  
  if (!this->enter_tx_mode()) return false;
  
  this->tx_count_++;
  ESP_LOGD(TAG, "Packet sent: %d bytes", data.size());
  return true;
}

bool CMT2300AComponent::enter_rx_mode() {
  return this->set_mode_(CMT2300A_MODE_RX);
}

bool CMT2300AComponent::enter_tx_mode() {
  return this->set_mode_(CMT2300A_MODE_TX);
}

bool CMT2300AComponent::enter_standby_mode() {
  return this->set_mode_(CMT2300A_MODE_STBY);
}

bool CMT2300AComponent::set_mode_(uint8_t mode) {
  this->write_register_(CMT2300A_REG_MODE_CTL, mode);
  
  if (this->wait_for_mode_(mode)) {
    this->current_mode_ = mode;
    return true;
  }
  
  ESP_LOGW(TAG, "Failed to set mode 0x%02X", mode);
  return false;
}

uint8_t CMT2300AComponent::get_mode_() {
  return this->read_register_(CMT2300A_REG_MODE_STA) & 0x0F;
}

bool CMT2300AComponent::wait_for_mode_(uint8_t mode, uint32_t timeout_ms) {
  uint32_t start = millis();
  while ((millis() - start) < timeout_ms) {
    if (this->get_mode_() == mode) return true;
    delay(1);
  }
  return false;
}

void CMT2300AComponent::handle_interrupt_() {
  uint8_t flags = this->read_interrupt_flags_();
  if (flags == 0) return;
  
  if (flags & CMT2300A_INT_RX_DONE) {
    if (flags & CMT2300A_INT_PKT_OK) {
      std::vector<uint8_t> data = this->read_fifo_();
      if (!data.empty() && this->receive_callback_) {
        this->rx_count_++;
        this->receive_callback_(data);
      }
    } else if (flags & CMT2300A_INT_CRC_ERR) {
      this->crc_error_count_++;
    }
    this->clear_fifo_();
    this->enter_rx_mode();
  }
  
  if (flags & CMT2300A_INT_TX_DONE) {
    this->enter_rx_mode();
  }
  
  this->clear_interrupt_flags_(flags);
}

uint8_t CMT2300AComponent::read_interrupt_flags_() {
  return this->read_register_(CMT2300A_REG_INT_FLAG);
}

void CMT2300AComponent::clear_interrupt_flags_(uint8_t flags) {
  this->write_register_(CMT2300A_REG_INT_CLR, flags);
}

bool CMT2300AComponent::reset_chip_() {
  this->write_register_(0x7F, 0xFF);
  delay(10);
  return true;
}

bool CMT2300AComponent::configure_frequency_() {
  uint32_t freq_reg = ((uint64_t)this->frequency_ << 16) / 26000000UL;
  this->write_register_(0x10, (freq_reg >> 16) & 0xFF);
  this->write_register_(0x11, (freq_reg >> 8) & 0xFF);
  this->write_register_(0x12, freq_reg & 0xFF);
  return true;
}

bool CMT2300AComponent::configure_data_rate_() {
  uint32_t rate_reg = 26000000UL / this->data_rate_;
  if (rate_reg > 0xFFFF) rate_reg = 0xFFFF;
  this->write_register_(0x20, (rate_reg >> 8) & 0xFF);
  this->write_register_(0x21, rate_reg & 0xFF);
  return true;
}

bool CMT2300AComponent::configure_packet_format_() {
  this->write_register_(0x30, 0x8A);
  this->write_register_(0x31, this->enable_crc_ ? 0x90 : 0x00);
  this->write_register_(0x33, 0xAA);
  this->write_register_(0x34, 0x55);
  this->write_register_(0x35, 0x00);
  this->write_register_(0x36, CMT2300A_FIFO_SIZE);
  return true;
}

bool CMT2300AComponent::configure_tx_power_() {
  uint8_t pa_power = (this->tx_power_ >= 20) ? 0x3F : (this->tx_power_ * 0x3F) / 20;
  this->write_register_(0x40, pa_power);
  return true;
}

bool CMT2300AComponent::calibrate_() {
  this->set_mode_(CMT2300A_MODE_CAL);
  uint32_t start = millis();
  while ((millis() - start) < 10) {
    if (this->get_mode_() == CMT2300A_MODE_STBY) return true;
    delay(1);
  }
  return false;
}

uint8_t CMT2300AComponent::get_rssi() {
  return this->read_register_(0x50);
}

}  // namespace cmt2300a
}  // namespace esphome

