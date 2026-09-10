#include "cmt2300a.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cmt2300a {

static const char *const TAG = "cmt2300a";

// Délai entre chaque demi-bit du protocole (IF_DELAY_US = 1 dans le driver de référence)
static const uint32_t IF_DELAY_US = 1;
static const uint32_t LINK_ATTEMPTS = 20;

// ---------------------------------------------------------------------------
// Bancs de registres par défaut, portés 1:1 depuis cmt2300a_params.h
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
// Bit-bang bas niveau -- port direct de if_send_byte()/if_read_byte() du .c de référence
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

  this->if_send_byte_(reg & 0x7F);  // r/w = 0 (écriture)
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

  this->if_send_byte_(reg | 0x80);  // r/w = 1 (lecture)

  // Le SDIO doit passer en entrée avant le front descendant de SCL qui suit
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
    this->write_reg_(i + base_addr, bank[i]);
  }
}

// ---------------------------------------------------------------------------
// Séquence haut niveau -- port des fonctions cmt2300a_*() du .c de référence
// ---------------------------------------------------------------------------
bool CMT2300AComponent::soft_reset_() {
  int attempts = LINK_ATTEMPTS;
  uint8_t status = STATE_INVALID;
  this->write_reg_(0x7F, 0xFF);
  while (status != STATE_SLEEP && attempts > 0) {
    delay(20);  // NOLINT -- équivalent du delay_us(20000) du driver de référence
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
bool CMT2300AComponent::reset_radio() {
  ESP_LOGW(TAG, "Reset radio matériel demandé");
  if (!this->soft_reset_()) {
    ESP_LOGE(TAG, "Reset radio : la puce ne répond pas");
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
  ESP_LOGV(TAG, "Pins configurées (CS=1, CLK=0, SDIO=1, FCS=1)");

  ESP_LOGD(TAG, "Soft reset...");
  if (!this->soft_reset_()) {
    ESP_LOGE(TAG, "Soft reset failed -- vérifie le câblage CLK/SDIO/CS/FCS");
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "Soft reset OK (état = SLEEP)");

  ESP_LOGD(TAG, "Passage en STBY...");
  if (!this->go_state_(GO_STBY, STATE_STBY)) {
    ESP_LOGE(TAG, "Impossible de passer en STBY");
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "STBY OK");

  if (this->external_mode_) {
    ESP_LOGD(TAG, "Mode externe actif -- test de présence de la puce uniquement");
    if (!this->is_chip_exist_()) {
      ESP_LOGE(TAG, "Puce CMT2300A non détectée (registre 0x48 illisible/incohérent)");
      this->mark_failed();
      return;
    }
    if (!this->go_state_(GO_SLEEP, STATE_SLEEP)) {
      ESP_LOGE(TAG, "Impossible de repasser en SLEEP après détection");
      this->mark_failed();
      return;
    }
    ESP_LOGCONFIG(TAG, "CMT2300A prêt (mode externe -- piloté par un autre composant)");
    return;
  }

  ESP_LOGD(TAG, "Config des bancs de registres...");
  this->config_reg_bank_(BANK_CMT_ADDR, BANK_CMT, BANK_CMT_SIZE);
  ESP_LOGV(TAG, "  Banc CMT écrit (base 0x%02X, %u octets)", BANK_CMT_ADDR, BANK_CMT_SIZE);
  this->config_reg_bank_(BANK_SYSTEM_ADDR, BANK_SYSTEM, BANK_SYSTEM_SIZE);
  ESP_LOGV(TAG, "  Banc System écrit (base 0x%02X, %u octets)", BANK_SYSTEM_ADDR, BANK_SYSTEM_SIZE);
  this->config_reg_bank_(BANK_FREQUENCY_ADDR, BANK_FREQUENCY, BANK_FREQUENCY_SIZE);
  ESP_LOGV(TAG, "  Banc Frequency écrit (base 0x%02X, %u octets)", BANK_FREQUENCY_ADDR, BANK_FREQUENCY_SIZE);
  this->config_reg_bank_(BANK_DATA_RATE_ADDR, BANK_DATA_RATE, BANK_DATA_RATE_SIZE);
  ESP_LOGV(TAG, "  Banc Data Rate écrit (base 0x%02X, %u octets)", BANK_DATA_RATE_ADDR, BANK_DATA_RATE_SIZE);
  this->config_reg_bank_(BANK_BASEBAND_ADDR, BANK_BASEBAND, BANK_BASEBAND_SIZE);
  ESP_LOGV(TAG, "  Banc Baseband écrit (base 0x%02X, %u octets)", BANK_BASEBAND_ADDR, BANK_BASEBAND_SIZE);
  this->config_reg_bank_(BANK_TX_ADDR, BANK_TX, BANK_TX_SIZE);
  ESP_LOGV(TAG, "  Banc Tx écrit (base 0x%02X, %u octets)", BANK_TX_ADDR, BANK_TX_SIZE);

  // LFOSC désactivée (comme le driver de référence)
  uint8_t sys2 = this->read_reg_(REG_CUS_SYS2);
  sys2 &= ~(MASK_LFOSC_RECAL_EN | MASK_LFOSC_CAL1_EN | MASK_LFOSC_CAL2_EN);
  this->write_reg_(REG_CUS_SYS2, sys2);
  uint8_t int2 = this->read_reg_(REG_CUS_INT2_CTL);
  int2 &= ~MASK_LFOSC_OUT_EN;
  this->write_reg_(REG_CUS_INT2_CTL, int2);
  ESP_LOGV(TAG, "LFOSC désactivée (SYS2=0x%02X, INT2_CTL=0x%02X)", sys2, int2);

  // RSTN_IN désactivé, CFG_RETAIN activé
  uint8_t sta = this->read_reg_(REG_CUS_MODE_STA);
  sta &= ~MASK_RSTN_IN_EN;
  sta |= MASK_CFG_RETAIN;
  this->write_reg_(REG_CUS_MODE_STA, sta);
  ESP_LOGV(TAG, "RSTN_IN désactivé, CFG_RETAIN activé (MODE_STA=0x%02X)", sta);

  uint8_t irq0 = this->clear_irq_flags_();
  ESP_LOGV(TAG, "Flags IRQ résiduels effacés (0x%02X)", irq0);

  ESP_LOGD(TAG, "Test de présence de la puce...");
  if (!this->is_chip_exist_()) {
    ESP_LOGE(TAG, "Puce CMT2300A non détectée (registre 0x48 illisible/incohérent)");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Puce CMT2300A détectée");

  if (!this->go_state_(GO_SLEEP, STATE_SLEEP)) {
    ESP_LOGE(TAG, "Impossible de repasser en SLEEP après config");
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "Retour en SLEEP OK");

  // GPIO2 -> INT1 (TX_DONE), GPIO3 -> INT2 (PKT_DONE) si déclarés dans le YAML
  uint32_t gpio_mask = 0;
  if (this->gpio2_pin_ != nullptr) gpio_mask |= GPIO2_SEL_INT1;
  if (this->gpio3_pin_ != nullptr) gpio_mask |= GPIO3_SEL_INT2;
  if (gpio_mask != 0) {
    if (!this->select_gpio_pins_mode_(gpio_mask)) {
      ESP_LOGW(TAG, "select_gpio_pins_mode a échoué");
    } else {
      ESP_LOGV(TAG, "select_gpio_pins_mode OK (mask=0x%02X)", (unsigned) gpio_mask);
    }
  }
  if (!this->map_gpio_to_irq_(INT_SEL_TX_DONE, INT_SEL_PKT_DONE)) {
    ESP_LOGW(TAG, "map_gpio_to_irq a échoué");
  } else {
    ESP_LOGV(TAG, "map_gpio_to_irq OK (INT1=TX_DONE, INT2=PKT_DONE)");
  }
  if (!this->set_irq_polar_(true)) {
    ESP_LOGW(TAG, "set_irq_polar a échoué");
  } else {
    ESP_LOGV(TAG, "set_irq_polar OK (actif haut)");
  }
  if (!this->enable_irq_(MASK_TX_DONE_EN | MASK_CRC_OK_EN | MASK_PKT_DONE_EN)) {
    ESP_LOGW(TAG, "enable_irq a échoué");
  } else {
    ESP_LOGV(TAG, "enable_irq OK (TX_DONE|CRC_OK|PKT_DONE)");
  }
  if (!this->set_merge_fifo_(false)) {
    ESP_LOGW(TAG, "set_merge_fifo a échoué");
  } else {
    ESP_LOGV(TAG, "set_merge_fifo OK (32/32 octets Tx/Rx)");
  }
  if (!this->set_fifo_threshold_reg_(this->fifo_threshold_)) {
    ESP_LOGW(TAG, "set_fifo_threshold a échoué");
  } else {
    ESP_LOGV(TAG, "set_fifo_threshold OK (%u)", this->fifo_threshold_);
  }
  if (!this->allow_receiving_any_nodeid_(this->accept_any_node_id_)) {
    ESP_LOGW(TAG, "allow_receiving_any_nodeid a échoué");
  } else {
    ESP_LOGV(TAG, "allow_receiving_any_nodeid OK (%s)", YESNO(this->accept_any_node_id_));
  }
  if (this->node_id_ != 0 || !this->accept_any_node_id_) {
    if (!this->set_node_id_reg_(this->node_id_)) {
      ESP_LOGW(TAG, "set_node_id a échoué");
    } else {
      ESP_LOGV(TAG, "set_node_id OK (0x%08X)", (unsigned) this->node_id_);
    }
  }

  if (this->gpio2_pin_ != nullptr) {
    this->gpio2_pin_->setup();
    this->gpio2_pin_->attach_interrupt(&CMT2300AComponent::gpio2_isr_, this, gpio::INTERRUPT_RISING_EDGE);
    ESP_LOGV(TAG, "Interruption GPIO2 attachée (front montant)");
  }
  if (this->gpio3_pin_ != nullptr) {
    this->gpio3_pin_->setup();
    this->gpio3_pin_->attach_interrupt(&CMT2300AComponent::gpio3_isr_, this, gpio::INTERRUPT_RISING_EDGE);
    ESP_LOGV(TAG, "Interruption GPIO3 attachée (front montant)");
  }

  ESP_LOGCONFIG(TAG, "CMT2300A prêt");
  if (!this->start_receive_()) {
    ESP_LOGW(TAG, "Premier démarrage de l'écoute RX en échec");
  } else {
    ESP_LOGV(TAG, "Écoute RX démarrée");
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
    ESP_LOGW(TAG, "Paquet TX invalide (%u octets, max %u)", (unsigned) data.size(), this->tx_fifo_size_);
    return false;
  }
  if (this->state_ == RadioState::TX_WAIT) {
    ESP_LOGW(TAG, "Transmission déjà en cours, envoi ignoré");
    return false;
  }

  if (!this->go_state_(GO_STBY, STATE_STBY)) {
    ESP_LOGW(TAG, "go_state(STBY) a échoué avant TX");
    return false;
  }
  this->clear_irq_flags_();

  this->write_reg_(REG_CUS_PKT15, (uint8_t) data.size());
  this->fifo_write_enable_();
  this->fifo_clear_tx_();
  this->write_fifo_(data.data(), data.size());

  this->tx_done_flag_ = false;
  if (!this->go_state_(GO_TX, STATE_TX)) {
    ESP_LOGW(TAG, "go_state(TX) a échoué");
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
        ESP_LOGW(TAG, "Timeout TX");
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
          ESP_LOGW(TAG, "Paquet reçu avec CRC invalide, ignoré");
        }
        this->start_receive_();
      } else if (millis() > this->state_deadline_) {
        // Pas de paquet reçu dans le délai : on relance simplement une écoute
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
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup a échoué -- puce non détectée ou câblage incorrect");
  }
}

}  // namespace cmt2300a
}  // namespace esphome
