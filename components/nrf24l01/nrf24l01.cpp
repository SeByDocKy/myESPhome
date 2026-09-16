#include "nrf24l01.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nrf24l01 {

static const char *const TAG = "nrf24l01";

// ---------------------------------------------------------------------------
// Low-level register access -- port of RF24::read_register()/write_register()
// (RF24.cpp), over real hardware SPI via the spi::SPIDevice mixin.
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
  // Pad up to payload_size in fixed-packet mode (like RF24::write_payload()
  // when dynamic_payloads_enabled is false) -- the inverter/peer on the other end
  // always expects payload_size bytes in this mode.
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
      // Corrupted packet (invalid width) -- like RF24::read(), flush the FIFO
      // instead of reading invalid data.
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
// High-level sequence -- ported from RF24::begin()/setPALevel()/setDataRate()/
// setCRCLength()/setRetries()/setAddressWidth()/openWritingPipe()/
// openReadingPipe()/startListening()/stopListening() (RF24.cpp)
// ---------------------------------------------------------------------------
void NRF24Component::power_up_() {
  uint8_t cfg = this->read_register_(REG_CONFIG);
  if (!(cfg & MASK_PWR_UP)) {
    this->write_register_(REG_CONFIG, cfg | MASK_PWR_UP);
    delay(5);  // NOLINT -- datasheet's Tpd2stby (oscillator settling)
  }
}

void NRF24Component::apply_pa_level_() {
  // Ported from RF24::setPALevel()/_pa_level_reg_value(): setup = (read & 0xF8) |
  // (level << 1) | lnaEnable. lnaEnable defaults to true in the real
  // library -- on a genuine nRF24L01+ chip this bit 0 has no documented
  // effect, but on SI24R1 clones (very common on cheap modules), it genuinely
  // changes the output level (+3dBm at PA_MAX). We set it unconditionally
  // to 1 to match the library's real default behavior rather than leaving it
  // at 0 by omission.
  uint8_t setup = this->read_register_(REG_RF_SETUP) & 0xF8;
  setup |= ((static_cast<uint8_t>(this->pa_level_) << 1) & 0b00000110) | 0b00000001;
  this->write_register_(REG_RF_SETUP, setup);
}

void NRF24Component::apply_data_rate_() {
  uint8_t setup = this->read_register_(REG_RF_SETUP);
  setup &= ~0b00101000;  // bit 5 = RF_DR_LOW, bit 3 = RF_DR_HIGH
  switch (this->data_rate_) {
    case RATE_250KBPS:
      setup |= (1 << 5);
      this->tx_delay_us_ = 505;
      break;
    case RATE_2MBPS:
      setup |= (1 << 3);
      this->tx_delay_us_ = 240;
      break;
    case RATE_1MBPS:
    default:
      this->tx_delay_us_ = 280;
      break;  // both bits at 0 = 1Mbps
  }
  this->write_register_(REG_RF_SETUP, setup);
}

void NRF24Component::apply_crc_length_() {
  uint8_t cfg = this->read_register_(REG_CONFIG);
  cfg &= ~(MASK_EN_CRC | MASK_CRCO);
  switch (this->crc_length_) {
    case CRC_DISABLED:
      // CRC can only be disabled if auto-ack is disabled on all
      // pipes (hardware constraint of the chip, like RF24::disableCRC()).
      if (!this->auto_ack_) {
        // nothing to set: EN_CRC=0
      } else {
        ESP_LOGW(TAG, "CRC not disabled: auto_ack is active (hardware constraint of the nRF24L01+)");
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
  // RX_ADDR_P0 must match the Tx address to receive hardware
  // acknowledgments (auto-ack) -- like RF24::openWritingPipe().
  this->write_register_(REG_RX_ADDR_P0, this->tx_address_, this->address_width_);
  this->write_register_(REG_TX_ADDR, this->tx_address_, this->address_width_);
  this->write_register_(REG_RX_PW_P0, this->payload_size_);
}

void NRF24Component::open_reading_pipe_(uint8_t pipe, const uint8_t *addr) {
  if (pipe > 5) return;
  if (pipe < 2) {
    this->write_register_(REG_RX_ADDR_P0 + pipe, addr, this->address_width_);
  } else {
    // Pipes 2-5 share the 4 high-order bytes of pipe 1's address --
    // only the LOW-order byte differs (hardware behavior of the chip). Verified
    // against RF24::openReadingPipe(): write_register(child_pipe[child],
    // reinterpret_cast<const uint8_t*>(&address), 1) -- i.e. byte 0 of the uint64_t
    // (LSB), which corresponds to addr[0] in our convention (see
    // serial_to_radio_address on the hm side). Writing addr[address_width-1] (as
    // before this fix) mistakenly took the high-order byte instead.
    this->write_register_(REG_RX_ADDR_P0 + pipe, &addr[0], 1);
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

  // Restore the receive address on pipe 0 (open_writing_pipe_() temporarily
  // uses it for auto-ack during transmissions).
  this->write_register_(REG_RX_ADDR_P0, this->rx_address_, this->address_width_);

  this->flush_rx_();
  this->flush_tx_();
  this->ce_pin_->digital_write(true);
  delayMicroseconds(130);  // datasheet's Trx2tx -- settling before listening
  this->listening_ = true;
}

void NRF24Component::stop_listening_() {
  this->ce_pin_->digital_write(false);
  delayMicroseconds(this->tx_delay_us_);
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
bool NRF24Component::reset_radio() {
  ESP_LOGW(TAG, "Hardware radio reset requested");
  this->ce_pin_->digital_write(false);
  uint8_t cfg = this->read_register_(REG_CONFIG);
  this->write_register_(REG_CONFIG, cfg & ~MASK_PWR_UP);
  delay(2);  // NOLINT -- Tpd du datasheet
  this->flush_rx_();
  this->flush_tx_();
  this->write_register_(REG_CONFIG, cfg | MASK_PWR_UP);
  delay(5);  // NOLINT -- Tpd2stby du datasheet
  uint8_t check = this->read_register_(REG_CONFIG);
  if (check == 0xFF) {
    ESP_LOGE(TAG, "Radio reset: chip is not responding");
    return false;
  }
  return true;
}

void NRF24Component::log_reg_(uint8_t reg, const char *label) {
  ESP_LOGV(TAG, "    %s (reg 0x%02X) = 0x%02X", label, reg, this->read_register_(reg));
}

void NRF24Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up NRF24...");
  this->spi_setup();

  this->ce_pin_->setup();
  this->ce_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->ce_pin_->digital_write(false);
  delay(5);  // NOLINT -- Tpor du datasheet (mise sous tension)

  this->write_register_(REG_CONFIG, 0);  // PWR_UP=0, PRIM_RX=0 -- known starting state
  this->log_reg_(REG_CONFIG, "CONFIG (reset)");

  this->apply_retries_();
  this->log_reg_(REG_SETUP_RETR, "SETUP_RETR");
  this->apply_data_rate_();
  this->log_reg_(REG_RF_SETUP, "RF_SETUP (data rate)");
  this->apply_crc_length_();
  this->log_reg_(REG_CONFIG, "CONFIG (CRC)");
  this->apply_address_width_();
  this->log_reg_(REG_SETUP_AW, "SETUP_AW");
  this->write_register_(REG_RF_CH, this->channel_ > 125 ? 125 : this->channel_);
  this->log_reg_(REG_RF_CH, "RF_CH");
  this->apply_pa_level_();
  this->log_reg_(REG_RF_SETUP, "RF_SETUP (PA level)");
#ifdef USE_SELECT
  if (this->pa_level_select_ != nullptr) {
    static const char *const kLevels[4] = {"min", "low", "high", "max"};
    uint8_t lvl = static_cast<uint8_t>(this->pa_level_);
    this->pa_level_select_->publish_state(kLevels[lvl > 3 ? 3 : lvl]);
  }
#endif

  this->write_register_(REG_EN_AA, this->auto_ack_ ? 0x3F : 0x00);
  this->log_reg_(REG_EN_AA, "EN_AA");
  this->write_register_(REG_EN_RXADDR, 0);  // pipes disabled, enabled by open_reading_pipe_()
  this->log_reg_(REG_EN_RXADDR, "EN_RXADDR (reset)");

  if (this->dynamic_payloads_) {
    this->write_register_(REG_FEATURE, FEATURE_EN_DPL);
    this->write_register_(REG_DYNPD, 0x3F);
  } else {
    this->write_register_(REG_FEATURE, 0);
    this->write_register_(REG_DYNPD, 0);
  }
  this->log_reg_(REG_FEATURE, "FEATURE");
  this->log_reg_(REG_DYNPD, "DYNPD");

  this->flush_rx_();
  this->flush_tx_();
  this->write_register_(REG_STATUS, STATUS_RX_DR | STATUS_TX_DS | STATUS_MAX_RT);
  this->log_reg_(REG_STATUS, "STATUS (clear)");

  this->power_up_();
  this->log_reg_(REG_CONFIG, "CONFIG (power up)");

  // Ported from RF24::isChipConnected(): SETUP_AW must equal address_width-2,
  // since we just wrote it ourselves via apply_address_width_() -- more
  // fiable qu'un heuristique sur CONFIG (0x00/0xFF), qui reste un bon indice
  // but not the method the real library actually uses.
  uint8_t expected_aw = (this->address_width_ >= 2) ? (this->address_width_ - 2) : 0;
  uint8_t aw_check = this->read_register_(REG_SETUP_AW);
  if (aw_check != expected_aw) {
    ESP_LOGE(TAG, "NRF24 chip not detected (SETUP_AW=0x%02X, expected 0x%02X) -- check SPI/CE wiring",
             aw_check, expected_aw);
    this->setup_failed_ = true;
    this->mark_failed();
    return;
  }

  if (this->external_mode_) {
    ESP_LOGCONFIG(TAG, "NRF24 ready (external mode -- driven by another component)");
    ESP_LOGI(TAG, "nrf24l01 setup ok");
    return;
  }

  this->open_writing_pipe_();
  this->open_reading_pipe_(1, this->rx_address_);
  this->start_listening_();

  ESP_LOGCONFIG(TAG, "NRF24 ready on channel %u", this->channel_);
  ESP_LOGI(TAG, "nrf24l01 setup ok");
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
  delayMicroseconds(15);  // minimum pulse (>10us) to trigger Tx
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
    ESP_LOGW(TAG, "Send failed: maximum retransmissions reached (no acknowledgment received)");
    this->flush_tx_();
  } else if (!done) {
    ESP_LOGW(TAG, "Timeout Tx (%ums)", static_cast<unsigned int>(timeout_ms));
    this->flush_tx_();
  }
  this->write_register_(REG_STATUS, STATUS_TX_DS | STATUS_MAX_RT);

  if (was_listening) this->start_listening_();
  return success;
}

void NRF24Component::dump_config() {
  static const char *const kPaLevels[4] = {"min", "low", "high", "max"};
  static const char *const kDataRates[3] = {"1mbps", "2mbps", "250kbps"};
  static const char *const kCrcLengths[3] = {"disabled", "8bit", "16bit"};

  uint8_t pa = static_cast<uint8_t>(this->pa_level_);
  uint8_t dr = static_cast<uint8_t>(this->data_rate_);
  uint8_t crc = static_cast<uint8_t>(this->crc_length_);

  ESP_LOGCONFIG(TAG, "NRF24:");
  LOG_PIN("  CE Pin: ", this->ce_pin_);
  if (this->irq_pin_ != nullptr) {
    LOG_PIN("  IRQ Pin: ", this->irq_pin_);
  }
  ESP_LOGCONFIG(TAG, "  Channel: %u (%u MHz)", this->channel_, 2400 + this->channel_);
  ESP_LOGCONFIG(TAG, "  PA level: %s", pa < 4 ? kPaLevels[pa] : "?");
  ESP_LOGCONFIG(TAG, "  Data rate: %s", dr < 3 ? kDataRates[dr] : "?");
  ESP_LOGCONFIG(TAG, "  CRC length: %s", crc < 3 ? kCrcLengths[crc] : "?");
  ESP_LOGCONFIG(TAG, "  Address width: %u", this->address_width_);
  ESP_LOGCONFIG(TAG, "  Auto ack: %s", YESNO(this->auto_ack_));
  ESP_LOGCONFIG(TAG, "  Retries: delay=%u count=%u", this->retry_delay_, this->retry_count_);
  ESP_LOGCONFIG(TAG, "  Payload size: %u%s", this->payload_size_, this->dynamic_payloads_ ? " (dynamic)" : "");
  if (this->external_mode_) {
    // This dump_config() runs during generic setup(), BEFORE hm:
    // (later AFTER_WIFI priority) reconfigures channel/data_rate/CRC/
    // adresses pour le protocole Hoymiles NRF -- les valeurs ci-dessus (sauf
    // pa_level, untouched by hm:) don't reflect the real final state yet.
    ESP_LOGCONFIG(TAG, "  External mode active (hm:) -- channel/data rate/CRC/addresses above");
    ESP_LOGCONFIG(TAG, "  will be overwritten by hm: after this point (see its own logs)");
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup failed -- chip not detected or incorrect wiring");
  }
}

}  // namespace nrf24l01
}  // namespace esphome
