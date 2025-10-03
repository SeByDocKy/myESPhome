#include "cmt2300a.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace cmt2300a {

static const char *const TAG = "cmt2300a";

void CMT2300AComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CMT2300A...");
  
  // Configuration des pins de contrôle
  if (this->cs_pin_ == nullptr || this->fcs_pin_ == nullptr) {
    ESP_LOGE(TAG, "CS or FCS pin not configured!");
    this->mark_failed();
    return;
  }
  
  this->cs_pin_->setup();
  this->cs_pin_->digital_write(true);  // CS idle high
  
  this->fcs_pin_->setup();
  this->fcs_pin_->digital_write(true); // FCS idle high
  
  if (this->gpio1_pin_ != nullptr) {
    this->gpio1_pin_->setup();
    this->gpio1_pin_->pin_mode(gpio::FLAG_INPUT);
  }
  
  if (this->gpio2_pin_ != nullptr) {
    this->gpio2_pin_->setup();
  }
  
  if (this->gpio3_pin_ != nullptr) {
    this->gpio3_pin_->setup();
  }
  
  // Initialisation SPI
  this->spi_setup();
  
  delay(10);
  
  // Reset du chip
  if (!this->reset_chip_()) {
    ESP_LOGE(TAG, "Failed to reset chip");
    this->mark_failed();
    return;
  }
  
  delay(50);
  
  // Vérification de la communication
  uint8_t chip_id = this->read_register_(0x7F);  // Registre ID chip
  ESP_LOGD(TAG, "Chip ID: 0x%02X", chip_id);
  
  // Configuration de base
  if (!this->enter_standby_mode()) {
    ESP_LOGE(TAG, "Failed to enter standby mode");
    this->mark_failed();
    return;
  }
  
  delay(10);
  
  // Configuration des paramètres radio
  if (!this->configure_frequency_()) {
    ESP_LOGE(TAG, "Failed to configure frequency");
    this->mark_failed();
    return;
  }
  
  if (!this->configure_data_rate_()) {
    ESP_LOGE(TAG, "Failed to configure data rate");
    this->mark_failed();
    return;
  }
  
  if (!this->configure_packet_format_()) {
    ESP_LOGE(TAG, "Failed to configure packet format");
    this->mark_failed();
    return;
  }
  
  if (!this->configure_tx_power_()) {
    ESP_LOGE(TAG, "Failed to configure TX power");
    this->mark_failed();
    return;
  }
  
  // Calibration
  if (!this->calibrate_()) {
    ESP_LOGW(TAG, "Calibration warning (may be normal)");
  }
  
  // Clear interruptions et FIFO
  this->clear_interrupt_flags_(0xFF);
  this->clear_fifo_();
  
  // Activation des interruptions
  this->write_register_(0x0B, 0x1F);  // Enable all interrupts
  
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
  ESP_LOGCONFIG(TAG, "CMT2300A:");
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
  this->enable();
  this->cs_pin_->digital_write(false);
  delayMicroseconds(1);
  
  this->write_byte(reg & 0x7F);  // R/W bit = 0 pour écriture
  this->write_byte(value);
  
  delayMicroseconds(1);
  this->cs_pin_->digital_write(true);
  this->disable();
}

uint8_t CMT2300AComponent::read_register_(uint8_t reg) {
  this->enable();
  this->cs_pin_->digital_write(false);
  delayMicroseconds(1);
  
  this->write_byte(0x80 | (reg & 0x7F));  // R/W bit = 1 pour lecture
  uint8_t value = this->read_byte();
  
  delayMicroseconds(1);
  this->cs_pin_->digital_write(true);
  this->disable();
  
  return value;
}

void CMT2300AComponent::write_fifo_(const std::vector<uint8_t> &data) {
  if (data.empty() || data.size() > CMT2300A_FIFO_SIZE) {
    ESP_LOGW(TAG, "Invalid FIFO write size: %d", data.size());
    return;
  }
  
  this->enable();
  this->fcs_pin_->digital_write(false);
  delayMicroseconds(1);
  
  this->write_byte(CMT2300A_FIFO_WR);
  this->write_byte(data.size());  // Longueur
  
  for (uint8_t byte : data) {
    this->write_byte(byte);
  }
  
  delayMicroseconds(1);
  this->fcs_pin_->digital_write(true);
  this->disable();
  
  ESP_LOGV(TAG, "FIFO write: %d bytes", data.size());
}

std::vector<uint8_t> CMT2300AComponent::read_fifo_() {
  std::vector<uint8_t> data;
  
  uint8_t length = this->get_fifo_length_();
  if (length == 0 || length > CMT2300A_FIFO_SIZE) {
    return data;
  }
  
  this->enable();
  this->fcs_pin_->digital_write(false);
  delayMicroseconds(1);
  
  this->write_byte(CMT2300A_FIFO_RD);
  uint8_t actual_length = this->read_byte();
  
  for (uint8_t i = 0; i < actual_length; i++) {
    data.push_back(this->read_byte());
  }
  
  delayMicroseconds(1);
  this->fcs_pin_->digital_write(true);
  this->disable();
  
  ESP_LOGV(TAG, "FIFO read: %d bytes", data.size());
  return data;
}

void CMT2300AComponent::clear_fifo_() {
  this->enable();
  this->fcs_pin_->digital_write(false);
  delayMicroseconds(1);
  
  this->write_byte(CMT2300A_FIFO_CLR);
  
  delayMicroseconds(1);
  this->fcs_pin_->digital_write(true);
  this->disable();
}

uint8_t CMT2300AComponent::get_fifo_length_() {
  return this->read_register_(CMT2300A_REG_FIFO_FLAG) & 0x7F;
}

bool CMT2300AComponent::send_packet(const std::vector<uint8_t> &data) {
  if (!this->chip_ready_ || data.empty()) {
    return false;
  }
  
  // Standby
  if (!this->enter_standby_mode()) {
    return false;
  }
  
  // Clear FIFO
  this->clear_fifo_();
  
  // Écriture données
  this->write_fifo_(data);
  
  // TX mode
  if (!this->enter_tx_mode()) {
    return false;
  }
  
  this->tx_count_++;
  ESP_LOGD(TAG, "Packet sent: %d bytes", data.size());
  return true;
}

bool CMT2300AComponent::enter_rx_mode() {
  if (this->set_mode_(CMT2300A_MODE_RX)) {
    ESP_LOGV(TAG, "Entered RX mode");
    return true;
  }
  return false;
}

bool CMT2300AComponent::enter_tx_mode() {
  if (this->set_mode_(CMT2300A_MODE_TX)) {
    ESP_LOGV(TAG, "Entered TX mode");
    return true;
  }
  return false;
}

bool CMT2300AComponent::enter_standby_mode() {
  if (this->set_mode_(CMT2300A_MODE_STBY)) {
    ESP_LOGV(TAG, "Entered Standby mode");
    return true;
  }
  return false;
}

bool CMT2300AComponent::enter_sleep_mode() {
  if (this->set_mode_(CMT2300A_MODE_SLEEP)) {
    ESP_LOGV(TAG, "Entered Sleep mode");
    return true;
  }
  return false;
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
  
  // RX Done
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
    
    // Clear FIFO et retour RX
    this->clear_fifo_();
    this->enter_rx_mode();
  }
  
  // TX Done
  if (flags & CMT2300A_INT_TX_DONE) {
    ESP_LOGV(TAG, "TX complete");
    this->enter_rx_mode();  // Retour en RX
  }
  
  // Clear flags
  this->clear_interrupt_flags_(flags);
}

uint8_t CMT2300AComponent::read_interrupt_flags_() {
  return this->read_register_(CMT2300A_REG_INT_FLAG);
}

void CMT2300AComponent::clear_interrupt_flags_(uint8_t flags) {
  this->write_register_(CMT2300A_REG_INT_CLR, flags);
}

bool CMT2300AComponent::reset_chip_() {
  // Soft reset
  this->write_register_(0x7F, 0xFF);
  delay(10);
  
  // Vérification reset
  uint8_t status = this->read_register_(CMT2300A_REG_MODE_STA);
  return (status & 0x80) == 0;  // Bit reset doit être 0
}

bool CMT2300AComponent::configure_frequency_() {
  // Calcul des registres de fréquence
  // Formule: freq_reg = (freq_hz * 2^16) / f_xosc
  // f_xosc = 26 MHz typiquement
  uint32_t freq_reg = ((uint64_t)this->frequency_ << 16) / 26000000UL;
  
  // Écriture sur 3 registres (24 bits)
  this->write_register_(0x10, (freq_reg >> 16) & 0xFF);
  this->write_register_(0x11, (freq_reg >> 8) & 0xFF);
  this->write_register_(0x12, freq_reg & 0xFF);
  
  ESP_LOGD(TAG, "Frequency configured: %u Hz (reg=0x%06X)", 
           this->frequency_, freq_reg);
  return true;
}

bool CMT2300AComponent::configure_data_rate_() {
  // Calcul du registre de débit
  // Formule: rate_reg = f_xosc / data_rate
  uint32_t rate_reg = 26000000UL / this->data_rate_;
  
  // Limitation à 16 bits
  if (rate_reg > 0xFFFF) rate_reg = 0xFFFF;
  
  // Écriture sur 2 registres
  this->write_register_(0x20, (rate_reg >> 8) & 0xFF);
  this->write_register_(0x21, rate_reg & 0xFF);
  
  ESP_LOGD(TAG, "Data rate configured: %u bps (reg=0x%04X)", 
           this->data_rate_, rate_reg);
  return true;
}

bool CMT2300AComponent::configure_packet_format_() {
  // Configuration du format de paquet
  
  // Registre 0x30: Packet Config
  // Bit 7: Variable length enable
  // Bit 6-4: Preamble length (011 = 4 bytes)
  // Bit 3-0: Sync word length (10 = 2 bytes)
  uint8_t pkt_config = 0x8A;  // Variable length, 4 byte preamble, 2 byte sync
  this->write_register_(0x30, pkt_config);
  
  // Registre 0x31: CRC et Whitening
  // Bit 7: CRC enable
  // Bit 6: Whitening enable
  // Bit 5-4: CRC type (01 = CRC-16)
  uint8_t crc_config = this->enable_crc_ ? 0x90 : 0x00;  // CRC-16 + Whitening
  this->write_register_(0x31, crc_config);
  
  // Registres 0x33-0x34: Sync Word (0xAA55 par défaut)
  this->write_register_(0x33, 0xAA);
  this->write_register_(0x34, 0x55);
  
  // Registre 0x35: Address filtering (disabled)
  this->write_register_(0x35, 0x00);
  
  // Registre 0x36: Max packet length
  this->write_register_(0x36, CMT2300A_FIFO_SIZE);
  
  ESP_LOGD(TAG, "Packet format configured (CRC: %s)", 
           this->enable_crc_ ? "enabled" : "disabled");
  return true;
}

bool CMT2300AComponent::configure_tx_power_() {
  // Configuration de la puissance TX
  // Le CMT2300A supporte 0-20 dBm
  
  uint8_t pa_power;
  if (this->tx_power_ <= 0) {
    pa_power = 0x00;
  } else if (this->tx_power_ >= 20) {
    pa_power = 0x3F;  // Max power
  } else {
    // Interpolation linéaire approximative
    pa_power = (this->tx_power_ * 0x3F) / 20;
  }
  
  // Registre 0x40: PA Power
  this->write_register_(0x40, pa_power);
  
  ESP_LOGD(TAG, "TX Power configured: %d dBm (reg=0x%02X)", 
           this->tx_power_, pa_power);
  return true;
}

bool CMT2300AComponent::calibrate_() {
  ESP_LOGD(TAG, "Starting calibration...");
  
  // Mode calibration
  this->set_mode_(CMT2300A_MODE_CAL);
  
  // Attente fin de calibration (max 5ms)
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
  // Lecture du RSSI (Received Signal Strength Indicator)
  return this->read_register_(0x50);  // Registre RSSI
}

}  // namespace cmt2300a
}  // namespace esphome