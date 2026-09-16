#include "cmt2300a.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cmt2300a {

static const char *const TAG = "cmt2300a";

// Delay between each half-bit of the protocol (IF_DELAY_US = 1 in the reference driver)
static const uint32_t IF_DELAY_US = 1;
static const uint32_t LINK_ATTEMPTS = 20;

// ---------------------------------------------------------------------------
// Default register banks, ported 1:1 from cmt2300a_params.h
// ---------------------------------------------------------------------------
static const uint8_t BANK_CMT[BANK_CMT_SIZE] = {
    0x00, 0x66, 0xEC, 0x1C, 0x70, 0x80, 0x14, 0x08, 0x91, 0x02, 0x02, 0xD0,
};
static const uint8_t BANK_SYSTEM[BANK_SYSTEM_SIZE] = {
    0xAE, 0x00, 0x35, 0x00, 0x00, 0xF4, 0x10, 0xE2, 0x42, 0xE0, 0x11, 0x81,
};
static const uint8_t BANK_FREQUENCY[BANK_FREQUENCY_SIZE] = {
    0x42, 0xDB, 0x00, 0x1D, 0x42, 0xC4, 0x4E, 0x1C,
};
static const uint8_t BANK_DATA_RATE[BANK_DATA_RATE_SIZE] = {
    0x3F, 0xF0, 0x23, 0x10, 0x63, 0x12, 0x09, 0x0A, 0x9F, 0x6C, 0x29, 0x29,
    0xC0, 0x04, 0x01, 0x53, 0x20, 0x00, 0xB4, 0x00, 0x00, 0x01, 0x00, 0x00,
};
static const uint8_t BANK_BASEBAND[BANK_BASEBAND_SIZE] = {
    0x12, 0x02, 0x00, 0xAA, 0x0F, 0x65, 0x19, 0xAA, 0xF0, 0xBC, 0x50, 0x24, 0xF1, 0x01, 0x1F,
    0x0D, 0x00, 0x00, 0x00, 0x00, 0xC1, 0xFF, 0xFF, 0x61, 0xFF, 0x02, 0x00, 0x1F, 0x10,
};
static const uint8_t BANK_TX[BANK_TX_SIZE] = {
    0x50, 0xE7, 0x12, 0x00, 0x00, 0x30, 0x00, 0x37, 0x0A, 0x7F, 0x7F,
};

// ---------------------------------------------------------------------------
// Low-level bit-bang -- direct port of if_send_byte()/if_read_byte() from the reference .c
// ---------------------------------------------------------------------------
void CMT2300AComponent::if_send_byte_(uint8_t data8) {
  for (int i = 0; i < 8; i++) {
    this->clk_pin_->digital_write(false);
    if (data8 & 0x80) {
      this->sdio_pin_->digital_write(true);
    } else {
      this->sdio_pin_->digital_write(false);
    }
    data8 <<= 1;
    delayMicroseconds(IF_DELAY_US);
    this->clk_pin_->digital_write(true);
    delayMicroseconds(IF_DELAY_US);
  }
}

uint8_t CMT2300AComponent::if_read_byte_() {
  uint8_t data8 = 0xFF;
  for (int i = 0; i < 8; i++) {
    this->clk_pin_->digital_write(false);
    delayMicroseconds(IF_DELAY_US);
    data8 <<= 1;
    this->clk_pin_->digital_write(true);
    if (this->sdio_pin_->digital_read()) {
      data8 |= 0x01;
    } else {
      data8 &= ~0x01;
    }
    delayMicroseconds(IF_DELAY_US);
  }
  return data8;
}

void CMT2300AComponent::write_reg_(uint8_t reg, uint8_t data) {
  this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->sdio_pin_->digital_write(true);
  this->clk_pin_->digital_write(false);
  this->fcs_pin_->digital_write(true);
  this->cs_pin_->digital_write(false);
  delayMicroseconds(2 * IF_DELAY_US);

  this->if_send_byte_(reg & 0x7F);  // r/w = 0 (write)
  this->if_send_byte_(data);

  this->clk_pin_->digital_write(false);
  delayMicroseconds(2 * IF_DELAY_US);
  this->cs_pin_->digital_write(true);
  this->sdio_pin_->digital_write(true);
  this->fcs_pin_->digital_write(true);
}

uint8_t CMT2300AComponent::read_reg_(uint8_t reg) {
  uint8_t value = 0;
  this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->sdio_pin_->digital_write(true);
  this->clk_pin_->digital_write(false);
  this->fcs_pin_->digital_write(true);
  this->cs_pin_->digital_write(false);
  delayMicroseconds(2 * IF_DELAY_US);

  this->if_send_byte_(reg | 0x80);  // r/w = 1 (read)

  // SDIO must switch to input before the following falling edge of SCL
  this->sdio_pin_->pin_mode(gpio::FLAG_INPUT);
  value = this->if_read_byte_();
  this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT);

  this->clk_pin_->digital_write(false);
  delayMicroseconds(2 * IF_DELAY_US);
  this->cs_pin_->digital_write(true);
  this->sdio_pin_->digital_write(true);
  this->fcs_pin_->digital_write(true);
  return value;
}

void CMT2300AComponent::write_fifo_(const uint8_t *buf, size_t len) {
  this->fcs_pin_->digital_write(true);
  this->cs_pin_->digital_write(true);
  this->clk_pin_->digital_write(false);
  this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT);

  for (size_t i = 0; i < len; i++) {
    this->fcs_pin_->digital_write(false);
    delayMicroseconds(3 * IF_DELAY_US);
    this->if_send_byte_(buf[i]);
    this->clk_pin_->digital_write(false);
    delayMicroseconds(4 * IF_DELAY_US);
    this->fcs_pin_->digital_write(true);
    delayMicroseconds(6 * IF_DELAY_US);
  }
  this->fcs_pin_->digital_write(true);
}

void CMT2300AComponent::read_fifo_(uint8_t *buf, size_t len) {
  this->fcs_pin_->digital_write(true);
  this->cs_pin_->digital_write(true);
  this->clk_pin_->digital_write(false);

  for (size_t i = 0; i < len; i++) {
    this->fcs_pin_->digital_write(false);
    delayMicroseconds(3 * IF_DELAY_US);
    this->sdio_pin_->pin_mode(gpio::FLAG_INPUT);
    buf[i] = this->if_read_byte_();
    this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->clk_pin_->digital_write(false);
    delayMicroseconds(4 * IF_DELAY_US);
    this->fcs_pin_->digital_write(true);
    delayMicroseconds(6 * IF_DELAY_US);
  }
  this->fcs_pin_->digital_write(true);
}

void CMT2300AComponent::fifo_write_enable_() {
  uint8_t tmp = this->read_reg_(REG_CUS_FIFO_CTL);
  tmp |= MASK_SPI_FIFO_RD_WR_SEL;
  if (this->is_fifo_merged_)
    tmp |= MASK_FIFO_RX_TX_SEL;
  this->write_reg_(REG_CUS_FIFO_CTL, tmp);
}

void CMT2300AComponent::fifo_read_enable_() {
  uint8_t tmp = this->read_reg_(REG_CUS_FIFO_CTL);
  tmp &= ~MASK_SPI_FIFO_RD_WR_SEL;
  if (this->is_fifo_merged_)
    tmp &= ~MASK_FIFO_RX_TX_SEL;
  this->write_reg_(REG_CUS_FIFO_CTL, tmp);
}

uint8_t CMT2300AComponent::fifo_clear_tx_() {
  uint8_t tmp = this->read_reg_(REG_CUS_FIFO_FLAG);
  this->write_reg_(REG_CUS_FIFO_CLR, MASK_FIFO_CLR_TX);
  return tmp;
}

uint8_t CMT2300AComponent::fifo_clear_rx_() {
  uint8_t tmp = this->read_reg_(REG_CUS_FIFO_FLAG);
  this->write_reg_(REG_CUS_FIFO_CLR, MASK_FIFO_CLR_RX);
  return tmp;
}

void CMT2300AComponent::config_reg_bank_(uint8_t base_addr, const uint8_t *bank, size_t len) {
  for (size_t i = 0; i < len; i++) {
    uint8_t reg = static_cast<uint8_t>(i + base_addr);
    this->write_reg_(reg, bank[i]);
    uint8_t readback = this->read_reg_(reg);
    if (readback == bank[i]) {
      ESP_LOGV(TAG, "    reg 0x%02X = 0x%02X (verified by readback)", reg, readback);
    } else {
      ESP_LOGW(TAG, "    reg 0x%02X: wrote 0x%02X but read back 0x%02X -- mismatch", reg, bank[i], readback);
    }
  }
}

// ---------------------------------------------------------------------------
// High-level sequence -- port of the cmt2300a_*() functions from the reference .c
// ---------------------------------------------------------------------------
bool CMT2300AComponent::soft_reset_() {
  int attempts = LINK_ATTEMPTS;
  uint8_t status = STATE_INVALID;
  this->write_reg_(0x7F, 0xFF);
  while (status != STATE_SLEEP && attempts > 0) {
    delay(20);  // NOLINT -- equivalent of the reference driver's delay_us(20000)
    status = this->get_state_();
    attempts--;
  }
  return status == STATE_SLEEP;
}

uint8_t CMT2300AComponent::get_state_() { return this->read_reg_(REG_CUS_MODE_STA) & MASK_CHIP_MODE_STA; }

bool CMT2300AComponent::go_state_(uint8_t go_state_mask, uint8_t desired_state) {
  int attempts = LINK_ATTEMPTS;
  uint8_t status = STATE_INVALID;
  this->write_reg_(REG_CUS_MODE_CTL, go_state_mask);
  while (status != desired_state && attempts > 0) {
    delayMicroseconds(1000);
    status = this->get_state_();
    attempts--;
  }
  return status == desired_state;
}

bool CMT2300AComponent::is_chip_exist_() {
  uint8_t back = this->read_reg_(REG_CUS_PKT17);
  this->write_reg_(REG_CUS_PKT17, 0xAA);
  uint8_t data = this->read_reg_(REG_CUS_PKT17);
  this->write_reg_(REG_CUS_PKT17, back);
  return data == 0xAA;
}

uint8_t CMT2300AComponent::clear_irq_flags_() {
  uint8_t polar = this->read_reg_(REG_CUS_INT1_CTL);
  polar = (polar & MASK_INT_POLAR) ? 1 : 0;

  uint8_t flag1 = this->read_reg_(REG_CUS_INT_FLAG);
  uint8_t flag2 = this->read_reg_(REG_CUS_INT_CLR1);
  if (polar) {
    flag1 = ~flag1;
    flag2 = ~flag2;
  }

  uint8_t clr1 = 0, clr2 = 0, ret = 0;
  if (MASK_LBD_FLG & flag1) clr2 |= MASK_LBD_CLR;
  if (MASK_COL_ERR_FLG & flag1) clr2 |= MASK_PKT_DONE_CLR;
  if (MASK_PKT_ERR_FLG & flag1) clr2 |= MASK_PKT_DONE_CLR;
  if (MASK_PREAM_OK_FLG & flag1) { clr2 |= MASK_PREAM_OK_CLR; ret |= MASK_PREAM_OK_FLG; }
  if (MASK_SYNC_OK_FLG & flag1) { clr2 |= MASK_SYNC_OK_CLR; ret |= MASK_SYNC_OK_FLG; }
  if (MASK_NODE_OK_FLG & flag1) { clr2 |= MASK_NODE_OK_CLR; ret |= MASK_NODE_OK_FLG; }
  if (MASK_CRC_OK_FLG & flag1) { clr2 |= MASK_CRC_OK_CLR; ret |= MASK_CRC_OK_FLG; }
  if (MASK_PKT_OK_FLG & flag1) { clr2 |= MASK_PKT_DONE_CLR; ret |= MASK_PKT_OK_FLG; }
  if (MASK_SL_TMO_FLG & flag2) { clr1 |= MASK_SL_TMO_CLR; ret |= MASK_SL_TMO_FLG; }
  if (MASK_RX_TMO_FLG & flag2) { clr1 |= MASK_RX_TMO_CLR; ret |= MASK_RX_TMO_FLG; }
  if (MASK_TX_DONE_FLG & flag2) { clr1 |= MASK_TX_DONE_CLR; ret |= MASK_TX_DONE_FLG; }

  this->write_reg_(REG_CUS_INT_CLR1, clr1);
  this->write_reg_(REG_CUS_INT_CLR2, clr2);

  if (polar) ret = ~ret;
  return ret;
}

bool CMT2300AComponent::select_gpio_pins_mode_(uint32_t mask) {
  if (!this->go_state_(GO_STBY, STATE_STBY)) return false;
  this->write_reg_(REG_CUS_IO_SEL, mask);
  return this->go_state_(GO_SLEEP, STATE_SLEEP);
}

bool CMT2300AComponent::map_gpio_to_irq_(uint8_t int1_mapping, uint8_t int2_mapping) {
  if (!this->go_state_(GO_STBY, STATE_STBY)) return false;

  uint8_t v1 = (int1_mapping & MASK_INT1_SEL) | (~MASK_INT1_SEL & this->read_reg_(REG_CUS_INT1_CTL));
  this->write_reg_(REG_CUS_INT1_CTL, v1);
  uint8_t v2 = (int2_mapping & MASK_INT2_SEL) | (~MASK_INT2_SEL & this->read_reg_(REG_CUS_INT2_CTL));
  this->write_reg_(REG_CUS_INT2_CTL, v2);

  return this->go_state_(GO_SLEEP, STATE_SLEEP);
}

bool CMT2300AComponent::set_irq_polar_(bool level_high) {
  if (!this->go_state_(GO_STBY, STATE_STBY)) return false;
  uint8_t tmp = this->read_reg_(REG_CUS_INT1_CTL);
  if (level_high) {
    tmp &= ~MASK_INT_POLAR;
  } else {
    tmp |= MASK_INT_POLAR;
  }
  this->write_reg_(REG_CUS_INT1_CTL, tmp);
  return this->go_state_(GO_SLEEP, STATE_SLEEP);
}

bool CMT2300AComponent::enable_irq_(uint8_t mask) {
  if (!this->go_state_(GO_STBY, STATE_STBY)) return false;
  this->write_reg_(REG_CUS_INT_EN, mask);
  return this->go_state_(GO_SLEEP, STATE_SLEEP);
}

bool CMT2300AComponent::set_merge_fifo_(bool merged) {
  if (!this->go_state_(GO_STBY, STATE_STBY)) return false;
  uint8_t tmp = this->read_reg_(REG_CUS_FIFO_CTL);
  if (merged) {
    tmp |= MASK_FIFO_MERGE_EN;
    this->is_fifo_merged_ = true;
    this->rx_fifo_size_ = 64;
    this->tx_fifo_size_ = 64;
  } else {
    tmp &= ~MASK_FIFO_MERGE_EN;
    this->is_fifo_merged_ = false;
    this->rx_fifo_size_ = 32;
    this->tx_fifo_size_ = 32;
  }
  this->write_reg_(REG_CUS_FIFO_CTL, tmp);
  return this->go_state_(GO_SLEEP, STATE_SLEEP);
}

bool CMT2300AComponent::set_fifo_threshold_reg_(uint8_t threshold) {
  if (!this->go_state_(GO_STBY, STATE_STBY)) return false;
  uint8_t tmp = this->read_reg_(REG_CUS_PKT29);
  tmp = (tmp & ~MASK_FIFO_TH) | (threshold & MASK_FIFO_TH);
  this->write_reg_(REG_CUS_PKT29, tmp);
  return this->go_state_(GO_SLEEP, STATE_SLEEP);
}

bool CMT2300AComponent::set_node_id_reg_(uint32_t node_id) {
  if (!this->go_state_(GO_STBY, STATE_STBY)) return false;
  this->write_reg_(REG_NODE_ID_0, (uint8_t) node_id);
  this->write_reg_(REG_NODE_ID_1, (uint8_t) (node_id >> 8));
  this->write_reg_(REG_NODE_ID_2, (uint8_t) (node_id >> 16));
  this->write_reg_(REG_NODE_ID_3, (uint8_t) (node_id >> 24));
  return this->go_state_(GO_SLEEP, STATE_SLEEP);
}

bool CMT2300AComponent::allow_receiving_any_nodeid_(bool allow) {
  if (!this->go_state_(GO_STBY, STATE_STBY)) return false;
  uint8_t reg = this->read_reg_(REG_CUS_PKT16);
  if (!allow) {
    reg &= ~MASK_NODE_ERR_MASK;
  } else {
    reg |= MASK_NODE_ERR_MASK;
  }
  this->write_reg_(REG_CUS_PKT16, reg);
  return this->go_state_(GO_SLEEP, STATE_SLEEP);
}

// ---------------------------------------------------------------------------
// ESPHome: setup / loop / dump_config
// ---------------------------------------------------------------------------
// dBm -> Tx_dBm_word table ported faithfully from CMT2300a::setPALevel()
// (lib/CMT2300a/cmt2300wrapper.cpp, OpenDTU commit 098691a -- "First step
// towards a modular CMT2300 driver similar to the NRF24 one").
void CMT2300AComponent::set_pa_level(int8_t dbm) {
  uint16_t tx_dbm_word;
  switch (dbm) {
    case -10: tx_dbm_word = 0x0501; break;
    case -9:  tx_dbm_word = 0x0601; break;
    case -8:  tx_dbm_word = 0x0701; break;
    case -7:  tx_dbm_word = 0x0801; break;
    case -6:  tx_dbm_word = 0x0901; break;
    case -5:  tx_dbm_word = 0x0A01; break;
    case -4:  tx_dbm_word = 0x0B01; break;
    case -3:  tx_dbm_word = 0x0C01; break;
    case -2:  tx_dbm_word = 0x0D01; break;
    case -1:  tx_dbm_word = 0x0E01; break;
    case 0:   tx_dbm_word = 0x1002; break;
    case 1:   tx_dbm_word = 0x1302; break;
    case 2:   tx_dbm_word = 0x1602; break;
    case 3:   tx_dbm_word = 0x1902; break;
    case 4:   tx_dbm_word = 0x1C02; break;
    case 5:   tx_dbm_word = 0x1F03; break;
    case 6:   tx_dbm_word = 0x2403; break;
    case 7:   tx_dbm_word = 0x2804; break;
    case 8:   tx_dbm_word = 0x2D04; break;
    case 9:   tx_dbm_word = 0x3305; break;
    case 10:  tx_dbm_word = 0x3906; break;
    case 11:  tx_dbm_word = 0x4107; break;
    case 12:  tx_dbm_word = 0x4908; break;
    case 13:  tx_dbm_word = 0x5309; break;
    case 14:  tx_dbm_word = 0x5E0B; break;
    case 15:  tx_dbm_word = 0x6C0C; break;
    case 16:  tx_dbm_word = 0x7D0C; break;
    // The following values require the "double" bit (register CUS_CMT4, bit0):
    case 17:  tx_dbm_word = 0x4A0C; break;
    case 18:  tx_dbm_word = 0x580F; break;
    case 19:  tx_dbm_word = 0x6B12; break;
    case 20:  tx_dbm_word = 0x8A18; break;
    default:
      ESP_LOGE(TAG, "invalid pa_level (%d dBm) -- must be between -10 and 20", dbm);
      return;
  }

  uint8_t cmt4 = this->read_reg_(BANK_CMT_ADDR + 4);  // CMT2300A_CUS_CMT4
  if (dbm > 16) {
    this->write_reg_(BANK_CMT_ADDR + 4, cmt4 | 0x01);   // set bit0 (double Tx)
  } else {
    this->write_reg_(BANK_CMT_ADDR + 4, cmt4 & 0xFE);   // clear bit0
  }
  this->write_reg_(BANK_TX_ADDR + 8, static_cast<uint8_t>(tx_dbm_word >> 8));   // CUS_TX8
  this->write_reg_(BANK_TX_ADDR + 9, static_cast<uint8_t>(tx_dbm_word & 0xFF));  // CUS_TX9

  this->pa_level_dbm_ = dbm;
  this->has_pa_level_ = true;
#ifdef USE_NUMBER
  if (this->pa_level_number_ != nullptr) {
    this->pa_level_number_->publish_state(dbm);
  }
#endif

  ESP_LOGD(TAG, "PA level set to %d dBm (Tx_dBm_word=0x%04X)", dbm, tx_dbm_word);
}

bool CMT2300AComponent::reset_radio() {
  ESP_LOGW(TAG, "Hardware radio reset requested");
  if (!this->soft_reset_()) {
    ESP_LOGE(TAG, "Radio reset: chip is not responding");
    return false;
  }
  this->external_radio_ready_ = false;
  return true;
}

void CMT2300AComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CMT2300A...");

  this->clk_pin_->setup();
  this->sdio_pin_->setup();
  this->cs_pin_->setup();
  this->fcs_pin_->setup();

  this->clk_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->cs_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->fcs_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->sdio_pin_->pin_mode(gpio::FLAG_OUTPUT);

  this->cs_pin_->digital_write(true);
  this->clk_pin_->digital_write(false);
  this->sdio_pin_->digital_write(true);
  this->fcs_pin_->digital_write(true);
  delayMicroseconds(20);
  ESP_LOGV(TAG, "Pins configured (CS=1, CLK=0, SDIO=1, FCS=1)");

  ESP_LOGD(TAG, "Soft reset...");
  if (!this->soft_reset_()) {
    ESP_LOGE(TAG, "Soft reset failed -- check CLK/SDIO/CS/FCS wiring");
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "Soft reset OK (state = SLEEP)");

  ESP_LOGD(TAG, "Switching to STBY...");
  if (!this->go_state_(GO_STBY, STATE_STBY)) {
    ESP_LOGE(TAG, "Unable to switch to STBY");
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "STBY OK");

  if (this->external_mode_) {
    ESP_LOGD(TAG, "External mode active -- chip presence test only");
    if (!this->is_chip_exist_()) {
      ESP_LOGE(TAG, "CMT2300A chip not detected (register 0x48 unreadable/inconsistent)");
      this->mark_failed();
      return;
    }
    if (!this->go_state_(GO_SLEEP, STATE_SLEEP)) {
      ESP_LOGE(TAG, "Unable to return to SLEEP after detection");
      this->mark_failed();
      return;
    }
    ESP_LOGCONFIG(TAG, "CMT2300A ready (external mode -- driven by another component)");
    ESP_LOGI(TAG, "cmt2300a setup ok");
    return;
  }

  ESP_LOGD(TAG, "Configuring register banks...");
  this->config_reg_bank_(BANK_CMT_ADDR, BANK_CMT, BANK_CMT_SIZE);
  ESP_LOGV(TAG, "  CMT bank written (base 0x%02X, %u bytes)", BANK_CMT_ADDR, BANK_CMT_SIZE);
  this->config_reg_bank_(BANK_SYSTEM_ADDR, BANK_SYSTEM, BANK_SYSTEM_SIZE);
  ESP_LOGV(TAG, "  System bank written (base 0x%02X, %u bytes)", BANK_SYSTEM_ADDR, BANK_SYSTEM_SIZE);
  this->config_reg_bank_(BANK_FREQUENCY_ADDR, BANK_FREQUENCY, BANK_FREQUENCY_SIZE);
  ESP_LOGV(TAG, "  Frequency bank written (base 0x%02X, %u bytes)", BANK_FREQUENCY_ADDR, BANK_FREQUENCY_SIZE);
  this->config_reg_bank_(BANK_DATA_RATE_ADDR, BANK_DATA_RATE, BANK_DATA_RATE_SIZE);
  ESP_LOGV(TAG, "  Data Rate bank written (base 0x%02X, %u bytes)", BANK_DATA_RATE_ADDR, BANK_DATA_RATE_SIZE);
  this->config_reg_bank_(BANK_BASEBAND_ADDR, BANK_BASEBAND, BANK_BASEBAND_SIZE);
  ESP_LOGV(TAG, "  Baseband bank written (base 0x%02X, %u bytes)", BANK_BASEBAND_ADDR, BANK_BASEBAND_SIZE);
  this->config_reg_bank_(BANK_TX_ADDR, BANK_TX, BANK_TX_SIZE);
  ESP_LOGV(TAG, "  Tx bank written (base 0x%02X, %u bytes)", BANK_TX_ADDR, BANK_TX_SIZE);
  if (this->has_pa_level_) {
    this->set_pa_level(this->pa_level_dbm_);
  }

  // LFOSC disabled (as in the reference driver)
  uint8_t sys2 = this->read_reg_(REG_CUS_SYS2);
  sys2 &= ~(MASK_LFOSC_RECAL_EN | MASK_LFOSC_CAL1_EN | MASK_LFOSC_CAL2_EN);
  this->write_reg_(REG_CUS_SYS2, sys2);
  uint8_t int2 = this->read_reg_(REG_CUS_INT2_CTL);
  int2 &= ~MASK_LFOSC_OUT_EN;
  this->write_reg_(REG_CUS_INT2_CTL, int2);
  ESP_LOGV(TAG, "LFOSC disabled (SYS2=0x%02X, INT2_CTL=0x%02X)", sys2, int2);

  // RSTN_IN disabled, CFG_RETAIN enabled
  uint8_t sta = this->read_reg_(REG_CUS_MODE_STA);
  sta &= ~MASK_RSTN_IN_EN;
  sta |= MASK_CFG_RETAIN;
  this->write_reg_(REG_CUS_MODE_STA, sta);
  ESP_LOGV(TAG, "RSTN_IN disabled, CFG_RETAIN enabled (MODE_STA=0x%02X)", sta);

  uint8_t irq0 = this->clear_irq_flags_();
  ESP_LOGV(TAG, "Residual IRQ flags cleared (0x%02X)", irq0);

  ESP_LOGD(TAG, "Testing chip presence...");
  if (!this->is_chip_exist_()) {
    ESP_LOGE(TAG, "CMT2300A chip not detected (register 0x48 unreadable/inconsistent)");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "CMT2300A chip detected");

  if (!this->go_state_(GO_SLEEP, STATE_SLEEP)) {
    ESP_LOGE(TAG, "Unable to return to SLEEP after config");
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "Return to SLEEP OK");

  // GPIO2 -> INT1 (TX_DONE), GPIO3 -> INT2 (PKT_DONE) if declared in YAML
  uint32_t gpio_mask = 0;
  if (this->gpio2_pin_ != nullptr) gpio_mask |= GPIO2_SEL_INT1;
  if (this->gpio3_pin_ != nullptr) gpio_mask |= GPIO3_SEL_INT2;
  if (gpio_mask != 0) {
    if (!this->select_gpio_pins_mode_(gpio_mask)) {
      ESP_LOGW(TAG, "select_gpio_pins_mode failed");
    } else {
      ESP_LOGV(TAG, "select_gpio_pins_mode OK (mask=0x%02X)", (unsigned) gpio_mask);
    }
  }
  if (!this->map_gpio_to_irq_(INT_SEL_TX_DONE, INT_SEL_PKT_DONE)) {
    ESP_LOGW(TAG, "map_gpio_to_irq failed");
  } else {
    ESP_LOGV(TAG, "map_gpio_to_irq OK (INT1=TX_DONE, INT2=PKT_DONE)");
  }
  if (!this->set_irq_polar_(true)) {
    ESP_LOGW(TAG, "set_irq_polar failed");
  } else {
    ESP_LOGV(TAG, "set_irq_polar OK (active high)");
  }
  if (!this->enable_irq_(MASK_TX_DONE_EN | MASK_CRC_OK_EN | MASK_PKT_DONE_EN)) {
    ESP_LOGW(TAG, "enable_irq failed");
  } else {
    ESP_LOGV(TAG, "enable_irq OK (TX_DONE|CRC_OK|PKT_DONE)");
  }
  if (!this->set_merge_fifo_(false)) {
    ESP_LOGW(TAG, "set_merge_fifo failed");
  } else {
    ESP_LOGV(TAG, "set_merge_fifo OK (32/32 bytes Tx/Rx)");
  }
  if (!this->set_fifo_threshold_reg_(this->fifo_threshold_)) {
    ESP_LOGW(TAG, "set_fifo_threshold failed");
  } else {
    ESP_LOGV(TAG, "set_fifo_threshold OK (%u)", this->fifo_threshold_);
  }
  if (!this->allow_receiving_any_nodeid_(this->accept_any_node_id_)) {
    ESP_LOGW(TAG, "allow_receiving_any_nodeid failed");
  } else {
    ESP_LOGV(TAG, "allow_receiving_any_nodeid OK (%s)", YESNO(this->accept_any_node_id_));
  }
  if (this->node_id_ != 0 || !this->accept_any_node_id_) {
    if (!this->set_node_id_reg_(this->node_id_)) {
      ESP_LOGW(TAG, "set_node_id failed");
    } else {
      ESP_LOGV(TAG, "set_node_id OK (0x%08X)", (unsigned) this->node_id_);
    }
  }

  if (this->gpio2_pin_ != nullptr) {
    this->gpio2_pin_->setup();
    this->gpio2_pin_->attach_interrupt(&CMT2300AComponent::gpio2_isr_, this, gpio::INTERRUPT_RISING_EDGE);
    ESP_LOGV(TAG, "GPIO2 interrupt attached (rising edge)");
  }
  if (this->gpio3_pin_ != nullptr) {
    this->gpio3_pin_->setup();
    this->gpio3_pin_->attach_interrupt(&CMT2300AComponent::gpio3_isr_, this, gpio::INTERRUPT_RISING_EDGE);
    ESP_LOGV(TAG, "GPIO3 interrupt attached (rising edge)");
  }

  ESP_LOGCONFIG(TAG, "CMT2300A ready");
  ESP_LOGI(TAG, "cmt2300a setup ok");
  if (!this->start_receive_()) {
    ESP_LOGW(TAG, "Initial RX listen start failed");
  } else {
    ESP_LOGV(TAG, "RX listening started");
  }
}

void IRAM_ATTR CMT2300AComponent::gpio2_isr_(CMT2300AComponent *self) { self->tx_done_flag_ = true; }
void IRAM_ATTR CMT2300AComponent::gpio3_isr_(CMT2300AComponent *self) { self->rx_packet_flag_ = true; }

bool CMT2300AComponent::start_receive_() {
  if (!this->go_state_(GO_STBY, STATE_STBY)) return false;
  this->clear_irq_flags_();
  this->fifo_read_enable_();
  this->fifo_clear_rx_();
  this->rx_packet_flag_ = false;
  if (!this->go_state_(GO_RX, STATE_RX)) return false;
  this->state_ = RadioState::RX_WAIT;
  this->state_deadline_ = millis() + 2000;
  return true;
}

bool CMT2300AComponent::send_packet(const std::vector<uint8_t> &data, uint32_t timeout_ms) {
  if (this->is_failed()) return false;
  if (data.empty() || data.size() > this->tx_fifo_size_) {
    ESP_LOGW(TAG, "Invalid TX packet (%u bytes, max %u)", (unsigned) data.size(), this->tx_fifo_size_);
    return false;
  }
  if (this->state_ == RadioState::TX_WAIT) {
    ESP_LOGW(TAG, "Transmission already in progress, send ignored");
    return false;
  }

  if (!this->go_state_(GO_STBY, STATE_STBY)) {
    ESP_LOGW(TAG, "go_state(STBY) failed before TX");
    return false;
  }
  this->clear_irq_flags_();

  this->write_reg_(REG_CUS_PKT15, (uint8_t) data.size());
  this->fifo_write_enable_();
  this->fifo_clear_tx_();
  this->write_fifo_(data.data(), data.size());

  this->tx_done_flag_ = false;
  if (!this->go_state_(GO_TX, STATE_TX)) {
    ESP_LOGW(TAG, "go_state(TX) failed");
    return false;
  }

  this->state_ = RadioState::TX_WAIT;
  this->state_deadline_ = millis() + timeout_ms;
  return true;
}

void CMT2300AComponent::loop() {
  if (this->is_failed() || this->external_mode_) return;

  switch (this->state_) {
    case RadioState::TX_WAIT: {
      if (this->tx_done_flag_) {
        this->tx_done_flag_ = false;
        this->clear_irq_flags_();
        this->go_state_(GO_SLEEP, STATE_SLEEP);
        this->state_ = RadioState::IDLE;
        this->tx_done_callback_.call();
        this->start_receive_();
      } else if (millis() > this->state_deadline_) {
        ESP_LOGW(TAG, "TX timeout");
        this->go_state_(GO_SLEEP, STATE_SLEEP);
        this->state_ = RadioState::IDLE;
        this->start_receive_();
      }
      break;
    }
    case RadioState::RX_WAIT: {
      if (this->rx_packet_flag_) {
        this->rx_packet_flag_ = false;
        this->go_state_(GO_STBY, STATE_STBY);
        this->read_fifo_(this->rx_buf_, this->rx_fifo_size_);
        uint8_t irq = this->clear_irq_flags_();
        this->go_state_(GO_SLEEP, STATE_SLEEP);
        this->state_ = RadioState::IDLE;

        if (irq & MASK_CRC_OK_FLG) {
          std::vector<uint8_t> data(this->rx_buf_, this->rx_buf_ + this->rx_fifo_size_);
          this->packet_callback_.call(data);
        } else {
          ESP_LOGW(TAG, "Packet received with invalid CRC, ignored");
        }
        this->start_receive_();
      } else if (millis() > this->state_deadline_) {
        // No packet received within the delay: simply restart listening
        this->go_state_(GO_SLEEP, STATE_SLEEP);
        this->state_ = RadioState::IDLE;
        this->start_receive_();
      }
      break;
    }
    case RadioState::IDLE:
    default:
      break;
  }
}

void CMT2300AComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CMT2300A:");
  LOG_PIN("  CLK Pin: ", this->clk_pin_);
  LOG_PIN("  SDIO Pin: ", this->sdio_pin_);
  LOG_PIN("  CS Pin: ", this->cs_pin_);
  LOG_PIN("  FCS Pin: ", this->fcs_pin_);
  if (this->gpio2_pin_ != nullptr) {
    LOG_PIN("  GPIO2 Pin: ", this->gpio2_pin_);
  }
  if (this->gpio3_pin_ != nullptr) {
    LOG_PIN("  GPIO3 Pin: ", this->gpio3_pin_);
  }
  ESP_LOGCONFIG(TAG, "  Node ID: 0x%08X", (unsigned) this->node_id_);
  ESP_LOGCONFIG(TAG, "  Accept any node ID: %s", YESNO(this->accept_any_node_id_));
  ESP_LOGCONFIG(TAG, "  FIFO threshold: %u", this->fifo_threshold_);
  if (this->external_mode_) {
    // Unlike nrf24l01/hm, the registers hms: reconfigures (frequency/data rate
    // banks) aren't generic config fields shown above -- nothing here becomes
    // stale after hms: runs, but the effective radio config (work frequency,
    // etc.) is entirely defined by hms: -- see its own logs for those values.
    ESP_LOGCONFIG(TAG, "  External mode active (hms:) -- frequency/data rate managed by hms:, see its logs");
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup failed -- chip not detected or incorrect wiring");
  }
}

}  // namespace cmt2300a
}  // namespace esphome
