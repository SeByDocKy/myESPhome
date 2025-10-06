#include "cmt2300a.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace cmt2300a {

static const char *const TAG = "cmt2300a";

void CMT2300AComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CMT2300A with Arduino SPI...");
  
  // Vérification des pins
  if (this->sclk_pin_ == nullptr || this->sdio_pin_ == nullptr || 
      this->cs_pin_ == nullptr || this->fcs_pin_ == nullptr) {
    ESP_LOGE(TAG, "Required pins not configured!");
    this->mark_failed();
    return;
  }
  
  // Configuration pins de contrôle
  this->cs_pin_->setup();
  this->cs_pin_->digital_write(true);
  
  this->fcs_pin_->setup();
  this->fcs_pin_->digital_write(true);
  
  if (this->gpio1_pin_ != nullptr) {
    this->gpio1_pin_->setup();
    this->gpio1_pin_->pin_mode(gpio::FLAG_INPUT);
  }
  
  // Initialisation SPI Arduino
  this->spi_ = new SPIClass(HSPI);
  this->spi_->begin(
    this->sclk_pin_->get_pin(),   // SCK
    this->sdio_pin_->get_pin(),   // MISO
    this->sdio_pin_->get_pin(),   // MOSI (même pin!)
    -1                            // CS géré manuellement
  );
  
  ESP_LOGD(TAG, "Arduino SPI initialized - SCLK: %d, SDIO: %d", 
           this->sclk_pin_->get_pin(), this->sdio_pin_->get_pin());
  
  delay(10);
  
  // Reset du chip
  if (!this->reset_chip_()) {
    ESP_LOGE(TAG, "Failed to reset chip");
    this->mark_failed();
    return;
  }
  
  delay(50);
  
  // Vérification communication
  uint8_t chip_id = this->read_register_(0x7F);
  ESP_LOGD(TAG, "Chip ID: 0x%02X", chip_id);
  
  if (chip_id == 0x00 || chip_id == 0xFF) {
    ESP_LOGW(TAG, "Chip ID looks invalid, but continuing...");
  }
  
  // Configuration de base
  if (!this->enter_standby_mode()) {
    ESP_LOGE(TAG, "Failed to enter standby mode");
    this->mark_failed();
    return;
  }
  
  delay(10);
  
  // Configuration des paramètres radio
  this->configure_frequency_();
  this->configure_data_rate_();
  this->configure_packet_format_();
  this->configure_tx_power_();
  this->calibrate_();
  
  // Clear interruptions et FIFO
  this->clear_interrupt_flags_(0xFF);
  this->clear_fifo_();
  
  // Activation des interruptions
  this->write_register_(0x0B, 0x1F);
  
  // Mode réception par défaut
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
  
  // Vérification interruption via GPIO1
  if (this->gpio1_pin_ != nullptr && this->gpio1_pin_->digital_read()) {
    this->handle_interrupt_();
  }
}

void CMT2300AComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CMT2300A (Arduino SPI):");
  LOG_PIN("  SCLK Pin: ", this->sclk_pin_);
  LOG_PIN("  SDIO Pin (bidirectional): ", this->sdio_pin_);
  LOG_PIN("  CS Pin: ", this->cs_pin_);
  LOG_PIN("  FCS Pin: ", this->fcs_pin_);
  LOG_PIN("  GPIO1 Pin: ", this->gpio1_pin_);
  ESP_LOGCONFIG(TAG, "  Frequency: %.3f MHz", this->frequency_ / 1e6);
  ESP_LOGCONFIG(TAG, "  Data Rate: %.1f kbps", this->data_rate_ / 1e3);
  ESP_LOGCONFIG(TAG, "  TX Power: %d dBm", this->tx_power_);
  ESP_LOGCONFIG(TAG, "  CRC: %s", this->enable_crc_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Status: %s", this->chip_ready_ ? "Ready" : "Failed");
  ESP_LOGCONFIG(TAG, "  Stats - TX: %u, RX: %u, CRC Errors: %u", 
                this->tx_count_, this->rx_count_, this->crc_error_count_);
}

void CMT2300AComponent::write_register_(uint8_t reg, uint8_t value) {
  this->cs_pin_->digital_write(false);
  this->spi_->beginTransaction(this->spi_settings_);
  
  this->spi_->transfer(reg & 0x7F);  // R/W = 0 pour écriture
  this->spi_->transfer(value);
  
  this->spi_->endTransaction();
  this->cs_pin_->digital_write(true);
  
  ESP_LOGVV(TAG, "Write reg 0x%02X = 0x%02X", reg, value);
}

uint8_t CMT2300AComponent::read_register_(uint8_t reg) {
  this->cs_pin_->digital_write(false);
  this->spi_->beginTransaction(this->spi_settings_);
  
  this->spi_->transfer(0x80 | (reg & 0x7F));  // R/W = 1 pour lecture
  uint8_t value = this->spi_->transfer(0x00); // Dummy byte pour lire
  
  this->spi_->endTransaction();
  this->cs_pin_->digital_write(true);
  
  ESP_LOGVV(TAG, "Read reg 0x%02X = 0x%02X", reg, value);
  return value;
}

void CMT2300AComponent::write_fifo_(const std::vector<uint8_t> &data) {
  if (data.empty() || data.size() > CMT2300A_FIFO_SIZE) {
    ESP_LOGW(TAG, "Invalid FIFO write size: %d", data.size());
    return;
  }
  
  this->fcs_pin_->digital_write(false);
  this->spi_->beginTransaction(this->spi_settings_);
  
  this->spi_->transfer(CMT2300A_FIFO_WR);
  this->spi_->transfer(data.size());
  
  for (uint8_t byte : data) {
    this->spi_->transfer(byte);
  }
  
  this->spi_->endTransaction();
  this->fcs_pin_->digital_write(true);
  
  ESP_LOGV(TAG, "FIFO write: %d bytes", data.size());
}

std::vector<uint8_t> CMT2300AComponent::read_fifo_() {
  std::vector<uint8_t> data;
  
  uint8_t length = this->get_fifo_length_();
  if (length == 0 || length > CMT2300A_FIFO_SIZE) {
    return data;
  }
  
  this->fcs_pin_->digital_write(false);
  this->spi_->beginTransaction(this->spi_settings_);
  
  this->spi_->transfer(CMT2300A_FIFO_RD);
  uint8_t actual_length = this->spi_->transfer(0x00);
  
  if (actual_length > 0 && actual_length <= CMT2300A_FIFO_SIZE) {
    data.reserve(actual_length);
    for (uint8_t i = 0; i < actual_length; i++) {
      data.push_back(this->spi_->transfer(0x00));
    }
  }
  
  this->spi_->endTransaction();
  this->fcs_pin_->digital_write(true);
  
  ESP_LOGV(TAG, "FIFO read: %d bytes", data.size());
  return data;
}

void CMT2300AComponent::clear_fifo_() {
  this->fcs_pin_->digital_write(false);
  this->spi_->beginTransaction(this->spi_settings_);
  
  this->spi_->transfer(CMT2300A_FIFO_CLR);
  
  this->spi_->endTransaction();
  this->fcs_pin_->digital_write(true);
}

uint8_t CMT2300AComponent::get_fifo_length_() {
  return this->read_register_(CMT2300A_REG_FIFO_FLAG) & 0x7F;
}

bool CMT2300AComponent::send_packet(const std::vector<uint8_t> &data) {
  if (!this->chip_ready_ || data.empty()) {
    return false;
  }
  
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

bool CMT2300AComponent::enter_sleep_mode() {
  return this->set_mode_(CMT2300A_MODE_SLEEP);
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
    if (this->get_mode_() == mode) {
      return true;
    }
    delay(1);
  }
  return false;
}

void CMT2300AComponent::handle_interrupt_() {
  uint8_t flags = this->read_interrupt_flags_();
  
  if (flags == 0) return;
  
  ESP_LOGV(TAG, "Interrupt flags: 0x%02X", flags);
  
  if (flags & CMT2300A_INT_RX_DONE) {
    if (flags & CMT2300A_INT_PKT_OK) {
      std::vector<uint8_t> data = this->read_fifo_();
      
      if (!data.empty() && this->receive_callback_) {
        this->rx_count_++;
        this->receive_callback_(data);
        ESP_LOGD(TAG, "RX packet: %d bytes", data.size());
      }
    } else if (flags & CMT2300A_INT_CRC_ERR) {
      this->crc_error_count_++;
      ESP_LOGD(TAG, "RX CRC error");
    }
    
    this->clear_fifo_();
    this->enter_rx_mode();
  }
  
  if (flags & CMT2300A_INT_TX_DONE) {
    ESP_LOGV(TAG, "TX complete");
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
  uint8_t status = this->read_register_(CMT2300A_REG_MODE_STA);
  return (status & 0x80) == 0;
}

bool CMT2300AComponent::configure_frequency_() {
  uint32_t freq_reg = ((uint64_t)this->frequency_ << 16) / 26000000UL;
  
  this->write_register_(0x10, (freq_reg >> 16) & 0xFF);
  this->write_register_(0x11, (freq_reg >> 8) & 0xFF);
  this->write_register_(0x12, freq_reg & 0xFF);
  
  ESP_LOGD(TAG, "Frequency configured: %u Hz", this->frequency_);
  return true;
}

bool CMT2300AComponent::configure_data_rate_() {
  uint32_t rate_reg = 26000000UL / this->data_rate_;
  if (rate_reg > 0xFFFF) rate_reg = 0xFFFF;
  
  this->write_register_(0x20, (rate_reg >> 8) & 0xFF);
  this->write_register_(0x21, rate_reg & 0xFF);
  
  ESP_LOGD(TAG, "Data rate configured: %u bps", this->data_rate_);
  return true;
}

bool CMT2300AComponent::configure_packet_format_() {
  this->write_register_(0x30, 0x8A);
  this->write_register_(0x31, this->enable_crc_ ? 0x90 : 0x00);
  this->write_register_(0x33, 0xAA);
  this->write_register_(0x34, 0x55);
  this->write_register_(0x35, 0x00);
  this->write_register_(0x36, CMT2300A_FIFO_SIZE);
  
  ESP_LOGD(TAG, "Packet format configured");
  return true;
}

bool CMT2300AComponent::configure_tx_power_() {
  uint8_t pa_power = (this->tx_power_ >= 20) ? 0x3F : (this->tx_power_ * 0x3F) / 20;
  this->write_register_(0x40, pa_power);
  
  ESP_LOGD(TAG, "TX Power configured: %d dBm", this->tx_power_);
  return true;
}

bool CMT2300AComponent::calibrate_() {
  this->set_mode_(CMT2300A_MODE_CAL);
  
  uint32_t start = millis();
  while ((millis() - start) < 10) {
    if (this->get_mode_() == CMT2300A_MODE_STBY) {
      ESP_LOGD(TAG, "Calibration complete");
      return true;
    }
    delay(1);
  }
  
  ESP_LOGW(TAG, "Calibration timeout");
  return false;
}

uint8_t CMT2300AComponent::get_rssi() {
  return this->read_register_(0x50);
}

}  // namespace cmt2300a
}  // namespace esphome