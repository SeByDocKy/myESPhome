#include "cmt2300a.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cmt2300a {

static const char *const TAG = "cmt2300a";

void CMT2300AComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CMT2300A...");
  
  // Configuration des pins
  this->cs_pin_->setup();
  this->cs_pin_->digital_write(true);
  
  this->fcs_pin_->setup();
  this->fcs_pin_->digital_write(true);
  
  if (this->gpio1_pin_ != nullptr) {
    this->gpio1_pin_->setup();
  }
  
  // Reset du chip
  this->reset_chip_();
  delay(10);
  
  // Configuration de base
  this->enter_standby_mode();
  delay(5);
  
  // Configuration fréquence
  this->configure_frequency_();
  
  // Configuration débit de données
  this->configure_data_rate_();
  
  // Configuration paquet
  this->configure_packet_();
  
  // Activation des interruptions
  this->write_register_(CMT2300A_REG_INT_EN, 
                        CMT2300A_INT_RX_DONE | CMT2300A_INT_TX_DONE | CMT2300A_INT_PKT_OK);
  
  // Mode réception par défaut
  this->enter_rx_mode();
  
  ESP_LOGCONFIG(TAG, "CMT2300A setup complete");
}

void CMT2300AComponent::loop() {
  // Vérification interruption (si GPIO1 configuré)
  if (this->gpio1_pin_ != nullptr && this->gpio1_pin_->digital_read()) {
    this->handle_interrupt_();
  }
}

void CMT2300AComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CMT2300A:");
  ESP_LOGCONFIG(TAG, "  Frequency: %d Hz", this->frequency_);
  ESP_LOGCONFIG(TAG, "  Data Rate: %d bps", this->data_rate_);
  LOG_PIN("  CS Pin: ", this->cs_pin_);
  LOG_PIN("  FCS Pin: ", this->fcs_pin_);
  LOG_PIN("  GPIO1 Pin: ", this->gpio1_pin_);
}

void CMT2300AComponent::write_register_(uint8_t reg, uint8_t value) {
  this->cs_pin_->digital_write(false);
  delayMicroseconds(1);
  
  // R/W bit = 0 (write) + 7 bits adresse
  this->write_byte(reg & 0x7F);
  this->write_byte(value);
  
  delayMicroseconds(1);
  this->cs_pin_->digital_write(true);
}

uint8_t CMT2300AComponent::read_register_(uint8_t reg) {
  this->cs_pin_->digital_write(false);
  delayMicroseconds(1);
  
  // R/W bit = 1 (read) + 7 bits adresse
  this->write_byte(0x80 | (reg & 0x7F));
  uint8_t value = this->read_byte();
  
  delayMicroseconds(1);
  this->cs_pin_->digital_write(true);
  
  return value;
}

void CMT2300AComponent::write_fifo_(const std::vector<uint8_t> &data) {
  if (data.empty() || data.size() > 255) {
    ESP_LOGW(TAG, "Invalid FIFO write size: %d", data.size());
    return;
  }
  
  this->fcs_pin_->digital_write(false);
  delayMicroseconds(1);
  
  // Commande write FIFO
  this->write_byte(CMT2300A_FIFO_WRITE);
  
  // Longueur
  this->write_byte(data.size());
  
  // Données
  for (uint8_t byte : data) {
    this->write_byte(byte);
  }
  
  delayMicroseconds(1);
  this->fcs_pin_->digital_write(true);
}

std::vector<uint8_t> CMT2300AComponent::read_fifo_(uint8_t length) {
  std::vector<uint8_t> data;
  
  this->fcs_pin_->digital_write(false);
  delayMicroseconds(1);
  
  // Commande read FIFO
  this->write_byte(CMT2300A_FIFO_READ);
  
  // Lecture longueur
  uint8_t actual_length = this->read_byte();
  
  // Lecture données
  for (uint8_t i = 0; i < actual_length && i < length; i++) {
    data.push_back(this->read_byte());
  }
  
  delayMicroseconds(1);
  this->fcs_pin_->digital_write(true);
  
  return data;
}

void CMT2300AComponent::clear_fifo_() {
  this->fcs_pin_->digital_write(false);
  delayMicroseconds(1);
  
  this->write_byte(CMT2300A_FIFO_CLEAR);
  
  delayMicroseconds(1);
  this->fcs_pin_->digital_write(true);
}

void CMT2300AComponent::enter_rx_mode() {
  this->write_register_(CMT2300A_REG_CHIP_MODE, CMT2300A_MODE_RX);
  ESP_LOGD(TAG, "Entered RX mode");
}

void CMT2300AComponent::enter_tx_mode() {
  this->write_register_(CMT2300A_REG_CHIP_MODE, CMT2300A_MODE_TX);
  ESP_LOGD(TAG, "Entered TX mode");
}

void CMT2300AComponent::enter_standby_mode() {
  this->write_register_(CMT2300A_REG_CHIP_MODE, CMT2300A_MODE_STBY);
  ESP_LOGD(TAG, "Entered Standby mode");
}

bool CMT2300AComponent::send_packet(const std::vector<uint8_t> &data) {
  if (data.empty()) return false;
  
  // Mode standby
  this->enter_standby_mode();
  delay(1);
  
  // Clear FIFO
  this->clear_fifo_();
  
  // Écriture données
  this->write_fifo_(data);
  
  // Mode TX
  this->enter_tx_mode();
  
  ESP_LOGD(TAG, "Packet sent: %d bytes", data.size());
  return true;
}

void CMT2300AComponent::handle_interrupt_() {
  uint8_t flags = this->read_interrupt_flags_();
  
  if (flags & CMT2300A_INT_RX_DONE) {
    ESP_LOGD(TAG, "RX Done interrupt");
    
    // Lecture FIFO
    std::vector<uint8_t> data = this->read_fifo_(255);
    
    if (!data.empty() && this->receive_callback_) {
      this->receive_callback_(data);
    }
    
    // Clear FIFO et retour RX
    this->clear_fifo_();
    this->enter_rx_mode();
  }
  
  if (flags & CMT2300A_INT_TX_DONE) {
    ESP_LOGD(TAG, "TX Done interrupt");
    // Retour en mode RX
    this->enter_rx_mode();
  }
  
  // Clear flags
  this->clear_interrupt_flags_();
}

uint8_t CMT2300AComponent::read_interrupt_flags_() {
  return this->read_register_(CMT2300A_REG_INT_FLAG);
}

void CMT2300AComponent::clear_interrupt_flags_() {
  this->write_register_(CMT2300A_REG_INT_FLAG, 0xFF);
}

void CMT2300AComponent::configure_frequency_() {
  // Configuration simplifiée - à adapter selon datasheet complet
  uint32_t freq_reg = (this->frequency_ * 65536ULL) / 26000000UL;
  
  // Écriture registres fréquence (à compléter selon datasheet)
  this->write_register_(0x10, (freq_reg >> 16) & 0xFF);
  this->write_register_(0x11, (freq_reg >> 8) & 0xFF);
  this->write_register_(0x12, freq_reg & 0xFF);
  
  ESP_LOGD(TAG, "Frequency configured: %d Hz", this->frequency_);
}

void CMT2300AComponent::configure_data_rate_() {
  // Configuration simplifiée du débit
  uint16_t rate_reg = 26000000UL / this->data_rate_;
  
  this->write_register_(0x20, (rate_reg >> 8) & 0xFF);
  this->write_register_(0x21, rate_reg & 0xFF);
  
  ESP_LOGD(TAG, "Data rate configured: %d bps", this->data_rate_);
}

void CMT2300AComponent::configure_packet_() {
  // Configuration format paquet
  // Variable length, CRC enable, address filtering off
  this->write_register_(0x30, 0x01);  // Variable length
  this->write_register_(0x31, 0x04);  // CRC enable
  this->write_register_(0x32, 0x00);  // Preamble length
  this->write_register_(0x33, 0xAA);  // Sync word 1
  this->write_register_(0x34, 0xAA);  // Sync word 2
}

void CMT2300AComponent::reset_chip_() {
  // Soft reset via registre
  this->write_register_(0x7F, 0xFF);
  delay(10);
  ESP_LOGD(TAG, "Chip reset");
}

}  // namespace cmt2300a
}  // namespace esphome
