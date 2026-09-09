#include "nrf24l01.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nrf24l01 {

static const char *const TAG = "nrf24l01";

// ---------------------------------------------------------------------------
// Accès registres bas niveau -- port de RF24::read_register()/write_register()
// (RF24.cpp), sur du vrai SPI matériel via le mixin spi::SPIDevice.
// ---------------------------------------------------------------------------
uint8_t NRF24Component::read_register_(uint8_t reg) {
  this->enable();
  this->transfer_byte(CMD_R_REGISTER | (CMD_REGISTER_MASK & reg));
  uint8_t result = this->transfer_byte(CMD_NOP);
  this->disable();
  return result;
}

void NRF24Component::read_register_(uint8_t reg, uint8_t *buf, uint8_t len) {
  this->enable();
  this->transfer_byte(CMD_R_REGISTER | (CMD_REGISTER_MASK & reg));
  for (uint8_t i = 0; i < len; i++) buf[i] = this->transfer_byte(CMD_NOP);
  this->disable();
}

uint8_t NRF24Component::write_register_(uint8_t reg, uint8_t value) {
  this->enable();
  uint8_t status = this->transfer_byte(CMD_W_REGISTER | (CMD_REGISTER_MASK & reg));
  this->transfer_byte(value);
  this->disable();
  return status;
}

uint8_t NRF24Component::write_register_(uint8_t reg, const uint8_t *buf, uint8_t len) {
  this->enable();
  uint8_t status = this->transfer_byte(CMD_W_REGISTER | (CMD_REGISTER_MASK & reg));
  for (uint8_t i = 0; i < len; i++) this->transfer_byte(buf[i]);
  this->disable();
  return status;
}

uint8_t NRF24Component::get_status_() {
  this->enable();
  uint8_t status = this->transfer_byte(CMD_NOP);
  this->disable();
  return status;
}

void NRF24Component::flush_rx_() {
  this->enable();
  this->transfer_byte(CMD_FLUSH_RX);
  this->disable();
}

void NRF24Component::flush_tx_() {
  this->enable();
  this->transfer_byte(CMD_FLUSH_TX);
  this->disable();
}

bool NRF24Component::write_payload_(const uint8_t *buf, uint8_t len) {
  if (len > this->payload_size_) len = this->payload_size_;
  this->enable();
  this->transfer_byte(CMD_W_TX_PAYLOAD);
  for (uint8_t i = 0; i < len; i++) this->transfer_byte(buf[i]);
  // Complète jusqu'à payload_size en mode paquets fixes (comme RF24::write_payload()
  // quand dynamic_payloads_enabled est faux) -- l'onduleur/le pair d'en face attend
  // toujours payload_size octets dans ce mode.
  if (!this->dynamic_payloads_) {
    for (uint8_t i = len; i < this->payload_size_; i++) this->transfer_byte(0);
  }
  this->disable();
  return true;
}

uint8_t NRF24Component::read_payload_(uint8_t *buf, uint8_t maxlen) {
  uint8_t len = this->payload_size_;
  if (this->dynamic_payloads_) {
    this->enable();
    this->transfer_byte(CMD_R_RX_PL_WID);
    len = this->transfer_byte(CMD_NOP);
    this->disable();
    if (len > 32) {
      // Paquet corrompu (largeur invalide) -- comme RF24::read(), on vide le FIFO
      // plutôt que de lire des données invalides.
      this->flush_rx_();
      return 0;
    }
  }
  if (len > maxlen) len = maxlen;

  this->enable();
  this->transfer_byte(CMD_R_RX_PAYLOAD);
  for (uint8_t i = 0; i < len; i++) buf[i] = this->transfer_byte(CMD_NOP);
  this->disable();
  return len;
}

// ---------------------------------------------------------------------------
// Séquence haut niveau -- portée de RF24::begin()/setPALevel()/setDataRate()/
// setCRCLength()/setRetries()/setAddressWidth()/openWritingPipe()/
// openReadingPipe()/startListening()/stopListening() (RF24.cpp)
// ---------------------------------------------------------------------------
void NRF24Component::power_up_() {
  uint8_t cfg = this->read_register_(REG_CONFIG);
  if (!(cfg & MASK_PWR_UP)) {
    this->write_register_(REG_CONFIG, cfg | MASK_PWR_UP);
    delay(5);  // NOLINT -- Tpd2stby du datasheet (montée en régime de l'oscillateur)
  }
}

void NRF24Component::apply_pa_level_() {
  uint8_t setup = this->read_register_(REG_RF_SETUP);
  setup &= ~0b00000110;  // bits [2:1] = RF_PWR
  setup |= (static_cast<uint8_t>(this->pa_level_) << 1) & 0b00000110;
  this->write_register_(REG_RF_SETUP, setup);
}

void NRF24Component::apply_data_rate_() {
  uint8_t setup = this->read_register_(REG_RF_SETUP);
  setup &= ~0b00101000;  // bit 5 = RF_DR_LOW, bit 3 = RF_DR_HIGH
  switch (this->data_rate_) {
    case RATE_250KBPS:
      setup |= (1 << 5);
      break;
    case RATE_2MBPS:
      setup |= (1 << 3);
      break;
    case RATE_1MBPS:
    default:
      break;  // les deux bits à 0 = 1Mbps
  }
  this->write_register_(REG_RF_SETUP, setup);
}

void NRF24Component::apply_crc_length_() {
  uint8_t cfg = this->read_register_(REG_CONFIG);
  cfg &= ~(MASK_EN_CRC | MASK_CRCO);
  switch (this->crc_length_) {
    case CRC_DISABLED:
      // Le CRC ne peut être désactivé que si l'auto-ack est désactivé sur toutes les
      // pipes (contrainte matérielle du chip, comme RF24::disableCRC()).
      if (!this->auto_ack_) {
        // rien à positionner : EN_CRC=0
      } else {
        ESP_LOGW(TAG, "CRC non désactivé : auto_ack est actif (contrainte matérielle du nRF24L01+)");
        cfg |= MASK_EN_CRC | MASK_CRCO;
      }
      break;
    case CRC_8BIT:
      cfg |= MASK_EN_CRC;
      break;
    case CRC_16BIT:
    default:
      cfg |= MASK_EN_CRC | MASK_CRCO;
      break;
  }
  this->write_register_(REG_CONFIG, cfg);
}

void NRF24Component::apply_retries_() {
  this->write_register_(REG_SETUP_RETR, ((this->retry_delay_ & 0x0F) << 4) | (this->retry_count_ & 0x0F));
}

void NRF24Component::apply_address_width_() {
  uint8_t w = this->address_width_;
  if (w < 3) w = 3;
  if (w > 5) w = 5;
  this->address_width_ = w;
  this->write_register_(REG_SETUP_AW, w - 2);
}

void NRF24Component::open_writing_pipe_() {
  // RX_ADDR_P0 doit correspondre à l'adresse Tx pour recevoir les accusés de
  // réception matériels (auto-ack) -- comme RF24::openWritingPipe().
  this->write_register_(REG_RX_ADDR_P0, this->tx_address_, this->address_width_);
  this->write_register_(REG_TX_ADDR, this->tx_address_, this->address_width_);
  this->write_register_(REG_RX_PW_P0, this->payload_size_);
}

void NRF24Component::open_reading_pipe_(uint8_t pipe, const uint8_t *addr) {
  if (pipe > 5) return;
  if (pipe < 2) {
    this->write_register_(REG_RX_ADDR_P0 + pipe, addr, this->address_width_);
  } else {
    // Les pipes 2-5 partagent les 4 octets de poids fort de l'adresse de la pipe 1 --
    // seul le dernier octet diffère (comportement matériel du chip).
    this->write_register_(REG_RX_ADDR_P0 + pipe, &addr[this->address_width_ - 1], 1);
  }
  this->write_register_(REG_RX_PW_P0 + pipe, this->payload_size_);
  uint8_t en_rxaddr = this->read_register_(REG_EN_RXADDR);
  this->write_register_(REG_EN_RXADDR, en_rxaddr | (1 << pipe));
}

void NRF24Component::start_listening_() {
  this->power_up_();
  uint8_t cfg = this->read_register_(REG_CONFIG);
  this->write_register_(REG_CONFIG, cfg | MASK_PRIM_RX);
  this->write_register_(REG_STATUS, STATUS_RX_DR | STATUS_TX_DS | STATUS_MAX_RT);

  // Restaure l'adresse de réception sur la pipe 0 (open_writing_pipe_() l'utilise
  // temporairement pour l'auto-ack pendant les transmissions).
  this->write_register_(REG_RX_ADDR_P0, this->rx_address_, this->address_width_);

  this->flush_rx_();
  this->flush_tx_();
  this->ce_pin_->digital_write(true);
  delayMicroseconds(130);  // Trx2tx du datasheet -- stabilisation avant écoute
  this->listening_ = true;
}

void NRF24Component::stop_listening_() {
  this->ce_pin_->digital_write(false);
  delayMicroseconds(130);
  this->flush_tx_();
  uint8_t cfg = this->read_register_(REG_CONFIG);
  this->write_register_(REG_CONFIG, cfg & ~MASK_PRIM_RX);
  this->listening_ = false;
}

bool NRF24Component::rx_available_() {
  uint8_t fifo = this->read_register_(REG_FIFO_STATUS);
  return !(fifo & FIFO_RX_EMPTY);
}

// ---------------------------------------------------------------------------
// ESPHome : setup / loop / dump_config
// ---------------------------------------------------------------------------
void NRF24Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up NRF24...");
  this->spi_setup();

  this->ce_pin_->setup();
  this->ce_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->ce_pin_->digital_write(false);
  delay(5);  // NOLINT -- Tpor du datasheet (mise sous tension)

  this->write_register_(REG_CONFIG, 0);  // PWR_UP=0, PRIM_RX=0 -- état connu de départ

  this->apply_retries_();
  this->apply_data_rate_();
  this->apply_crc_length_();
  this->apply_address_width_();
  this->write_register_(REG_RF_CH, this->channel_ > 125 ? 125 : this->channel_);
  this->apply_pa_level_();

  this->write_register_(REG_EN_AA, this->auto_ack_ ? 0x3F : 0x00);
  this->write_register_(REG_EN_RXADDR, 0);  // pipes désactivées, activées par open_reading_pipe_()

  if (this->dynamic_payloads_) {
    this->write_register_(REG_FEATURE, FEATURE_EN_DPL);
    this->write_register_(REG_DYNPD, 0x3F);
  } else {
    this->write_register_(REG_FEATURE, 0);
    this->write_register_(REG_DYNPD, 0);
  }

  this->flush_rx_();
  this->flush_tx_();
  this->write_register_(REG_STATUS, STATUS_RX_DR | STATUS_TX_DS | STATUS_MAX_RT);

  this->power_up_();

  uint8_t cfg_check = this->read_register_(REG_CONFIG);
  if (cfg_check == 0xFF || cfg_check == 0x00) {
    // 0xFF (bus non connecté) ou 0x00 (PWR_UP n'a pas pris, alors qu'on vient de
    // l'activer) -- dans les deux cas, la puce ne répond pas correctement.
    ESP_LOGE(TAG, "Puce NRF24 non détectée (CONFIG=0x%02X) -- vérifie le câblage SPI/CE", cfg_check);
    this->setup_failed_ = true;
    this->mark_failed();
    return;
  }

  if (this->external_mode_) {
    ESP_LOGCONFIG(TAG, "NRF24 prêt (mode externe -- piloté par un autre composant)");
    return;
  }

  this->open_writing_pipe_();
  this->open_reading_pipe_(1, this->rx_address_);
  this->start_listening_();

  ESP_LOGCONFIG(TAG, "NRF24 prêt sur le canal %u", this->channel_);
}

void NRF24Component::loop() {
  if (this->is_failed() || this->external_mode_) return;
  if (!this->listening_) return;

  if (this->rx_available_()) {
    uint8_t buf[32];
    uint8_t len = this->read_payload_(buf, sizeof(buf));
    this->write_register_(REG_STATUS, STATUS_RX_DR);
    if (len > 0) {
      std::vector<uint8_t> data(buf, buf + len);
      this->packet_callback_.call(data);
    }
  }
}

bool NRF24Component::send_packet(const std::vector<uint8_t> &data, uint32_t timeout_ms) {
  if (this->is_failed() || data.empty()) return false;

  bool was_listening = this->listening_;
  if (was_listening) this->stop_listening_();

  this->flush_tx_();
  this->write_payload_(data.data(), static_cast<uint8_t>(data.size()));

  this->ce_pin_->digital_write(true);
  delayMicroseconds(15);  // impulsion minimale (>10us) pour déclencher la Tx
  this->ce_pin_->digital_write(false);

  uint32_t start = millis();
  uint8_t status = 0;
  bool done = false;
  while (millis() - start < timeout_ms) {
    status = this->get_status_();
    if (status & (STATUS_TX_DS | STATUS_MAX_RT)) {
      done = true;
      break;
    }
  }

  bool success = done && (status & STATUS_TX_DS);
  if (status & STATUS_MAX_RT) {
    ESP_LOGW(TAG, "Envoi échoué : nombre maximal de retransmissions atteint (pas d'accusé reçu)");
    this->flush_tx_();
  } else if (!done) {
    ESP_LOGW(TAG, "Timeout Tx (%ums)", timeout_ms);
    this->flush_tx_();
  }
  this->write_register_(REG_STATUS, STATUS_TX_DS | STATUS_MAX_RT);

  if (was_listening) this->start_listening_();
  return success;
}

void NRF24Component::dump_config() {
  ESP_LOGCONFIG(TAG, "NRF24:");
  LOG_PIN("  CE Pin: ", this->ce_pin_);
  if (this->irq_pin_ != nullptr) {
    LOG_PIN("  IRQ Pin: ", this->irq_pin_);
  }
  ESP_LOGCONFIG(TAG, "  Channel: %u (%u MHz)", this->channel_, 2400 + this->channel_);
  ESP_LOGCONFIG(TAG, "  PA level: %u", this->pa_level_);
  ESP_LOGCONFIG(TAG, "  Data rate: %u", this->data_rate_);
  ESP_LOGCONFIG(TAG, "  CRC length: %u", this->crc_length_);
  ESP_LOGCONFIG(TAG, "  Address width: %u", this->address_width_);
  ESP_LOGCONFIG(TAG, "  Auto ack: %s", YESNO(this->auto_ack_));
  ESP_LOGCONFIG(TAG, "  Retries: delay=%u count=%u", this->retry_delay_, this->retry_count_);
  ESP_LOGCONFIG(TAG, "  Payload size: %u%s", this->payload_size_, this->dynamic_payloads_ ? " (dynamic)" : "");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup a échoué -- puce non détectée ou câblage incorrect");
  }
}

}  // namespace nrf24l01
}  // namespace esphome
