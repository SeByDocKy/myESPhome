#include "hms.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <ctime>

namespace esphome {
namespace hms {

static const char *const TAG = "hms";

// ---------------------------------------------------------------------------
// CRC -- porté 1:1 depuis OpenDTU lib/Hoymiles/src/crc.cpp
// ---------------------------------------------------------------------------
static uint8_t crc8(const uint8_t *buf, uint8_t len) {
  uint8_t crc = 0x00;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= buf[i];
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc << 1) ^ ((crc & 0x80) ? 0x01 : 0x00);
    }
  }
  return crc;
}

static uint16_t crc16(const uint8_t *buf, uint8_t len, uint16_t start = 0xffff) {
  uint16_t crc = start;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= buf[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      uint8_t shift = crc & 0x0001;
      crc >>= 1;
      if (shift) crc ^= 0xA001;
    }
  }
  return crc;
}

// Codes de vérification de fragments -- portés depuis inverters/InverterAbstract.h
static const uint8_t FRAGMENT_ALL_MISSING_RESEND = 255;
static const uint8_t FRAGMENT_ALL_MISSING_TIMEOUT = 254;
static const uint8_t FRAGMENT_RETRANSMIT_TIMEOUT = 253;
static const uint8_t FRAGMENT_HANDLE_ERROR = 252;
static const uint8_t FRAGMENT_OK = 0;
static const uint8_t MAX_RESEND_COUNT = 4;      // commands/CommandAbstract.h
static const uint8_t MAX_RETRANSMIT_COUNT = 5;  // commands/CommandAbstract.h

// ---------------------------------------------------------------------------
// Tables d'octets -- portées 1:1 depuis inverters/HMS_1CH.cpp, HMS_2CH.cpp, HMS_4CH.cpp
// (les champs FLD_IRR ne sont pas portés : nécessitent une config de puissance crête
//  par string non exposée en v1 -- voir README)
// ---------------------------------------------------------------------------
static const byteAssign_t HMS_1CH_TABLE[] = {
    {TYPE_DC, CH0, FLD_UDC, 2, 2, 10, false, 1},
    {TYPE_DC, CH0, FLD_IDC, 4, 2, 100, false, 2},
    {TYPE_DC, CH0, FLD_PDC, 6, 2, 10, false, 1},
    {TYPE_DC, CH0, FLD_YD, 12, 2, 1, false, 0},
    {TYPE_DC, CH0, FLD_YT, 8, 4, 1000, false, 3},

    {TYPE_AC, CH0, FLD_UAC, 14, 2, 10, false, 1},
    {TYPE_AC, CH0, FLD_IAC, 22, 2, 100, false, 2},
    {TYPE_AC, CH0, FLD_PAC, 18, 2, 10, false, 1},
    {TYPE_AC, CH0, FLD_Q, 20, 2, 10, true, 1},
    {TYPE_AC, CH0, FLD_F, 16, 2, 100, false, 2},
    {TYPE_AC, CH0, FLD_PF, 24, 2, 1000, false, 3},

    {TYPE_INV, CH0, FLD_T, 26, 2, 10, true, 1},
    {TYPE_INV, CH0, FLD_EVT_LOG, 28, 2, 1, false, 0},

    {TYPE_INV, CH0, FLD_YD, CALC_TOTAL_YD, 0, CMD_CALC, false, 0},
    {TYPE_INV, CH0, FLD_YT, CALC_TOTAL_YT, 0, CMD_CALC, false, 3},
    {TYPE_INV, CH0, FLD_PDC, CALC_TOTAL_PDC, 0, CMD_CALC, false, 1},
    {TYPE_INV, CH0, FLD_EFF, CALC_TOTAL_EFF, 0, CMD_CALC, false, 3},
};

static const byteAssign_t HMS_2CH_TABLE[] = {
    {TYPE_DC, CH0, FLD_UDC, 2, 2, 10, false, 1},
    {TYPE_DC, CH0, FLD_IDC, 6, 2, 100, false, 2},
    {TYPE_DC, CH0, FLD_PDC, 10, 2, 10, false, 1},
    {TYPE_DC, CH0, FLD_YT, 14, 4, 1000, false, 3},
    {TYPE_DC, CH0, FLD_YD, 22, 2, 1, false, 0},

    {TYPE_DC, CH1, FLD_UDC, 4, 2, 10, false, 1},
    {TYPE_DC, CH1, FLD_IDC, 8, 2, 100, false, 2},
    {TYPE_DC, CH1, FLD_PDC, 12, 2, 10, false, 1},
    {TYPE_DC, CH1, FLD_YT, 18, 4, 1000, false, 3},
    {TYPE_DC, CH1, FLD_YD, 24, 2, 1, false, 0},

    {TYPE_AC, CH0, FLD_UAC, 26, 2, 10, false, 1},
    {TYPE_AC, CH0, FLD_IAC, 34, 2, 100, false, 2},
    {TYPE_AC, CH0, FLD_PAC, 30, 2, 10, false, 1},
    {TYPE_AC, CH0, FLD_Q, 32, 2, 10, true, 1},
    {TYPE_AC, CH0, FLD_F, 28, 2, 100, false, 2},
    {TYPE_AC, CH0, FLD_PF, 36, 2, 1000, false, 3},

    {TYPE_INV, CH0, FLD_T, 38, 2, 10, true, 1},
    {TYPE_INV, CH0, FLD_EVT_LOG, 40, 2, 1, false, 0},

    {TYPE_INV, CH0, FLD_YD, CALC_TOTAL_YD, 0, CMD_CALC, false, 0},
    {TYPE_INV, CH0, FLD_YT, CALC_TOTAL_YT, 0, CMD_CALC, false, 3},
    {TYPE_INV, CH0, FLD_PDC, CALC_TOTAL_PDC, 0, CMD_CALC, false, 1},
    {TYPE_INV, CH0, FLD_EFF, CALC_TOTAL_EFF, 0, CMD_CALC, false, 3},
};

static const byteAssign_t HMS_4CH_TABLE[] = {
    {TYPE_DC, CH0, FLD_UDC, 2, 2, 10, false, 1},
    {TYPE_DC, CH0, FLD_IDC, 6, 2, 100, false, 2},
    {TYPE_DC, CH0, FLD_PDC, 10, 2, 10, false, 1},
    {TYPE_DC, CH0, FLD_YD, 22, 2, 1, false, 0},
    {TYPE_DC, CH0, FLD_YT, 14, 4, 1000, false, 3},

    {TYPE_DC, CH1, FLD_UDC, 4, 2, 10, false, 1},
    {TYPE_DC, CH1, FLD_IDC, 8, 2, 100, false, 2},
    {TYPE_DC, CH1, FLD_PDC, 12, 2, 10, false, 1},
    {TYPE_DC, CH1, FLD_YD, 24, 2, 1, false, 0},
    {TYPE_DC, CH1, FLD_YT, 18, 4, 1000, false, 3},

    {TYPE_DC, CH2, FLD_UDC, 26, 2, 10, false, 1},
    {TYPE_DC, CH2, FLD_IDC, 30, 2, 100, false, 2},
    {TYPE_DC, CH2, FLD_PDC, 34, 2, 10, false, 1},
    {TYPE_DC, CH2, FLD_YD, 46, 2, 1, false, 0},
    {TYPE_DC, CH2, FLD_YT, 38, 4, 1000, false, 3},

    {TYPE_DC, CH3, FLD_UDC, 28, 2, 10, false, 1},
    {TYPE_DC, CH3, FLD_IDC, 32, 2, 100, false, 2},
    {TYPE_DC, CH3, FLD_PDC, 36, 2, 10, false, 1},
    {TYPE_DC, CH3, FLD_YD, 48, 2, 1, false, 0},
    {TYPE_DC, CH3, FLD_YT, 42, 4, 1000, false, 3},

    {TYPE_AC, CH0, FLD_UAC, 50, 2, 10, false, 1},
    {TYPE_AC, CH0, FLD_IAC, 58, 2, 100, false, 2},
    {TYPE_AC, CH0, FLD_PAC, 54, 2, 10, false, 1},
    {TYPE_AC, CH0, FLD_Q, 56, 2, 10, true, 1},
    {TYPE_AC, CH0, FLD_F, 52, 2, 100, false, 2},
    {TYPE_AC, CH0, FLD_PF, 60, 2, 1000, false, 3},

    {TYPE_INV, CH0, FLD_T, 62, 2, 10, true, 1},
    {TYPE_INV, CH0, FLD_EVT_LOG, 64, 2, 1, false, 0},

    {TYPE_INV, CH0, FLD_YD, CALC_TOTAL_YD, 0, CMD_CALC, false, 0},
    {TYPE_INV, CH0, FLD_YT, CALC_TOTAL_YT, 0, CMD_CALC, false, 3},
    {TYPE_INV, CH0, FLD_PDC, CALC_TOTAL_PDC, 0, CMD_CALC, false, 1},
    {TYPE_INV, CH0, FLD_EFF, CALC_TOTAL_EFF, 0, CMD_CALC, false, 3},
};

// ---------------------------------------------------------------------------
// Bancs de registres CMT2300A -- portés depuis lib/CMT2300a/cmt2300a_params_860.h
// et cmt2300a_params_900.h (config Hoymiles, différente du banc générique 433MHz)
// ---------------------------------------------------------------------------
static const uint8_t BANK_CMT_860[] = {0x00, 0x66, 0xEC, 0x1C, 0x70, 0x80, 0x14, 0x08, 0x11, 0x02, 0x02, 0x00};
static const uint8_t BANK_SYSTEM_860[] = {0xAE, 0xE0, 0x35, 0x00, 0x00, 0xF4, 0x10, 0xE2, 0x42, 0x20, 0x0C, 0x81};
static const uint8_t BANK_FREQ_860[] = {0x42, 0x32, 0xCF, 0x82, 0x42, 0x27, 0x76, 0x12};
static const uint8_t BANK_RATE_860[] = {0xA6, 0xC9, 0x20, 0x20, 0xD2, 0x35, 0x0C, 0x0A, 0x9F, 0x4B, 0x29, 0x29,
                                         0xC0, 0x14, 0x05, 0x53, 0x10, 0x00, 0xB4, 0x00, 0x00, 0x01, 0x00, 0x00};
static const uint8_t BANK_BB_860[] = {0x12, 0x1E, 0x00, 0xAA, 0x06, 0x00, 0x00, 0x00, 0x00, 0x48, 0x5A, 0x48, 0x4D,
                                       0x01, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3, 0x00, 0x00, 0x60, 0xFF, 0x00,
                                       0x00, 0x1F, 0x10};
static const uint8_t BANK_TX_860[] = {0x70, 0x4D, 0x06, 0x00, 0x07, 0x50, 0x00, 0x53, 0x09, 0x3F, 0x7F};

static const uint8_t BANK_CMT_900[] = {0x00, 0x66, 0xEC, 0x1C, 0x70, 0x80, 0x14, 0x08, 0x11, 0x02, 0x02, 0x00};
static const uint8_t BANK_SYSTEM_900[] = {0xAE, 0xE0, 0x35, 0x00, 0x00, 0xF4, 0x10, 0xE2, 0x42, 0x20, 0x0C, 0x81};
static const uint8_t BANK_FREQ_900[] = {0x45, 0x46, 0x0A, 0x84, 0x45, 0x3B, 0xB1, 0x13};
static const uint8_t BANK_RATE_900[] = {0xA6, 0xC9, 0x20, 0x20, 0xD2, 0x35, 0x0C, 0x0B, 0x9F, 0x4B, 0x29, 0x29,
                                         0xC0, 0x14, 0x05, 0x53, 0x10, 0x00, 0xB4, 0x00, 0x00, 0x01, 0x00, 0x00};
static const uint8_t BANK_BB_900[] = {0x12, 0x1E, 0x00, 0xAA, 0x06, 0x00, 0x00, 0x00, 0x00, 0x48, 0x5A, 0x48, 0x4D,
                                       0x01, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3, 0x00, 0x00, 0x60, 0xFF, 0x00,
                                       0x00, 0x1F, 0x10};
static const uint8_t BANK_TX_900[] = {0x70, 0x4D, 0x06, 0x00, 0x07, 0x50, 0x00, 0x53, 0x09, 0x3F, 0x7F};

// ---------------------------------------------------------------------------
// Adressage -- porté depuis commands/CommandAbstract.cpp (convertSerialToPacketId)
// ---------------------------------------------------------------------------
static void serial_to_packet_id(uint8_t out4[4], uint64_t serial) {
  out4[0] = static_cast<uint8_t>(serial >> 24);
  out4[1] = static_cast<uint8_t>(serial >> 16);
  out4[2] = static_cast<uint8_t>(serial >> 8);
  out4[3] = static_cast<uint8_t>(serial >> 0);
}

// ---------------------------------------------------------------------------
// Décodage du numéro de série -> modèle / nombre de canaux DC
// Préfixes portés depuis inverters/HMS_1CH.cpp, HMS_2CH.cpp, HMS_4CH.cpp
// ---------------------------------------------------------------------------
bool HMSComponent::decode_serial_() {
  uint16_t pre = static_cast<uint16_t>((this->inverter_serial_ >> 32) & 0xFFFF);
  if (pre == 0x1124) {
    this->dc_channel_count_ = 1;
    this->byte_assignment_ = HMS_1CH_TABLE;
    this->byte_assignment_size_ = sizeof(HMS_1CH_TABLE) / sizeof(HMS_1CH_TABLE[0]);
    this->type_name_ = "HMS-300/350/400/450/500-1T";
  } else if (pre == 0x1143 || pre == 0x1144 || pre == 0x1410 || pre == 0x114a) {
    this->dc_channel_count_ = 2;
    this->byte_assignment_ = HMS_2CH_TABLE;
    this->byte_assignment_size_ = sizeof(HMS_2CH_TABLE) / sizeof(HMS_2CH_TABLE[0]);
    this->type_name_ = "HMS-600/700/800/900/1000-2T";
  } else if (pre == 0x1164 || pre == 0x1166 || pre == 0x1420) {
    this->dc_channel_count_ = 4;
    this->byte_assignment_ = HMS_4CH_TABLE;
    this->byte_assignment_size_ = sizeof(HMS_4CH_TABLE) / sizeof(HMS_4CH_TABLE[0]);
    this->type_name_ = "HMS-1600/1800/2000-4T";
  } else {
    return false;
  }

  this->expected_byte_count_ = 0;
  for (uint8_t i = 0; i < this->byte_assignment_size_; i++) {
    if (this->byte_assignment_[i].div == CMD_CALC) continue;
    uint8_t end = this->byte_assignment_[i].start + this->byte_assignment_[i].num;
    if (end > this->expected_byte_count_) this->expected_byte_count_ = end;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Génération du numéro de série DTU -- porté depuis src/Utils.cpp (generateDtuSerial)
// ---------------------------------------------------------------------------
uint64_t HMSComponent::generate_dtu_serial_() {
  uint8_t mac[6];
  get_mac_address_raw(mac);

  // Reconstruit un "chip id" 24 bits à partir des 3 premiers octets de la MAC,
  // dans le même esprit que Utils::getChipId() (dérivé de l'eFuse MAC Arduino).
  uint32_t chip_id = (static_cast<uint32_t>(mac[0])) | (static_cast<uint32_t>(mac[1]) << 8) |
                      (static_cast<uint32_t>(mac[2]) << 16);

  uint64_t dtu_id = 0;
  dtu_id |= 0x199900000000ULL;  // Catégorie produit (1 = micro-onduleur)
  dtu_id |= 0x80000000ULL;      // Année de prod (8 = 2022, valeur fixe comme OpenDTU)
  dtu_id |= 0x0100000ULL;       // Semaine de prod (fixe = semaine 1)

  for (uint8_t i = 0; i < 5; i++) {
    dtu_id |= static_cast<uint64_t>(chip_id % 10) << (i * 4);
    chip_id /= 10;
  }
  return dtu_id;
}

// ---------------------------------------------------------------------------
// Init radio Hoymiles -- porté depuis lib/CMT2300a/cmt2300wrapper.cpp (_init_radio)
// ---------------------------------------------------------------------------
bool HMSComponent::init_radio_() {
  if (!this->radio_->go_state(cmt2300a::GO_STBY, cmt2300a::STATE_STBY)) return false;

  const uint8_t *cmt_bank, *sys_bank, *freq_bank, *rate_bank, *bb_bank, *tx_bank;
  if (this->frequency_band_ == FrequencyBand::US_900) {
    cmt_bank = BANK_CMT_900;
    sys_bank = BANK_SYSTEM_900;
    freq_bank = BANK_FREQ_900;
    rate_bank = BANK_RATE_900;
    bb_bank = BANK_BB_900;
    tx_bank = BANK_TX_900;
  } else {
    cmt_bank = BANK_CMT_860;
    sys_bank = BANK_SYSTEM_860;
    freq_bank = BANK_FREQ_860;
    rate_bank = BANK_RATE_860;
    bb_bank = BANK_BB_860;
    tx_bank = BANK_TX_860;
  }

  this->radio_->config_reg_bank(cmt2300a::BANK_CMT_ADDR, cmt_bank, cmt2300a::BANK_CMT_SIZE);
  this->radio_->config_reg_bank(cmt2300a::BANK_SYSTEM_ADDR, sys_bank, cmt2300a::BANK_SYSTEM_SIZE);
  this->radio_->config_reg_bank(cmt2300a::BANK_FREQUENCY_ADDR, freq_bank, cmt2300a::BANK_FREQUENCY_SIZE);
  this->radio_->config_reg_bank(cmt2300a::BANK_DATA_RATE_ADDR, rate_bank, cmt2300a::BANK_DATA_RATE_SIZE);
  this->radio_->config_reg_bank(cmt2300a::BANK_BASEBAND_ADDR, bb_bank, cmt2300a::BANK_BASEBAND_SIZE);
  this->radio_->config_reg_bank(cmt2300a::BANK_TX_ADDR, tx_bank, cmt2300a::BANK_TX_SIZE);

  // CFG_RETAIN activé / RSTN_IN désactivé (CUS_MODE_STA, 0x61) -- fait par CMT2300A_Init()
  // côté OpenDTU, mais absent du chemin external_mode de cmt2300a (qui ne fait que le
  // strict minimum : reset + détection puce).
  uint8_t mode_sta = this->radio_->read_reg(cmt2300a::REG_CUS_MODE_STA);
  mode_sta |= cmt2300a::MASK_CFG_RETAIN;
  mode_sta &= ~cmt2300a::MASK_RSTN_IN_EN;
  this->radio_->write_reg(cmt2300a::REG_CUS_MODE_STA, mode_sta);

  // LOCKING_EN (CUS_EN_CTL, 0x62, bit 0x20) -- verrouillage du synthétiseur RF.
  // Sans ce bit le PLL peut ne jamais se verrouiller correctement sur la fréquence
  // configurée : c'était manquant dans la première version du portage.
  uint8_t en_ctl = this->radio_->read_reg(0x62);
  en_ctl |= 0x20;
  this->radio_->write_reg(0x62, en_ctl);

  // Note : CMT2300A_Init() désactive aussi LFOSC (registre SYS2, 0x0D) chez OpenDTU,
  // mais cette étape s'exécute AVANT l'écriture du banc "System" -- qui réécrit ensuite
  // ce même registre avec sa propre valeur (LFOSC Calibration = On selon le RFPDK). La
  // désactivation explicite est donc sans effet dans le firmware réel ; on ne la reproduit
  // pas ici pour ne pas diverger (le banc System_860/900 fait déjà foi).

  // xosc_aac_code[2:0] = 2 (registre CUS_CMT10, adresse 0x09)
  uint8_t cmt10 = this->radio_->read_reg(0x09);
  this->radio_->write_reg(0x09, (cmt10 & ~0x07) | 0x02);

  // GPIO2 -> INT1 (TX_DONE), GPIO3 -> INT2 (PKT_OK)
  this->radio_->write_reg(cmt2300a::REG_CUS_IO_SEL, cmt2300a::GPIO2_SEL_INT1 | cmt2300a::GPIO3_SEL_INT2);
  this->radio_->write_reg(cmt2300a::REG_CUS_INT1_CTL, cmt2300a::INT_SEL_TX_DONE);
  this->radio_->write_reg(cmt2300a::REG_CUS_INT2_CTL, cmt2300a::INT_SEL_PKT_OK);
  // TX_DONE_EN | PREAM_OK_EN | SYNC_OK_EN | CRC_OK_EN | PKT_DONE_EN
  this->radio_->write_reg(cmt2300a::REG_CUS_INT_EN,
                           cmt2300a::MASK_TX_DONE_EN | 0x10 | 0x08 | cmt2300a::MASK_CRC_OK_EN |
                               cmt2300a::MASK_PKT_DONE_EN);

  this->radio_->write_reg(cmt2300a::REG_CUS_FREQ_OFS, FH_OFFSET);

  // FIFO fusionné 64 octets (obligatoire pour le protocole Hoymiles)
  uint8_t fifo_ctl = this->radio_->read_reg(cmt2300a::REG_CUS_FIFO_CTL);
  fifo_ctl |= cmt2300a::MASK_FIFO_MERGE_EN;
  this->radio_->write_reg(cmt2300a::REG_CUS_FIFO_CTL, fifo_ctl);

  this->radio_->clear_irq_flags();

  return this->radio_->go_state(cmt2300a::GO_SLEEP, cmt2300a::STATE_SLEEP);
}

uint32_t HMSComponent::frequency_from_channel_(uint8_t channel) const {
  uint32_t base = (this->frequency_band_ == FrequencyBand::US_900) ? 900000000UL : 860000000UL;
  return base + static_cast<uint32_t>(channel) * FH_OFFSET * CMT_ONE_STEP_SIZE;
}

uint8_t HMSComponent::channel_from_frequency_(uint32_t freq_hz) const {
  uint32_t base = (this->frequency_band_ == FrequencyBand::US_900) ? 900000000UL : 860000000UL;
  uint32_t width = FH_OFFSET * CMT_ONE_STEP_SIZE;
  if (freq_hz < base) return 0;
  return static_cast<uint8_t>((freq_hz - base) / width);
}

void HMSComponent::switch_to_channel_(uint8_t channel) {
  this->work_channel_ = channel;
  this->radio_->write_reg(cmt2300a::REG_CUS_FREQ_CHNL, channel);
}

void HMSComponent::switch_to_frequency_(uint32_t freq_hz) { this->switch_to_channel_(this->channel_from_frequency_(freq_hz)); }

// ---------------------------------------------------------------------------
// Emission/réception bas niveau -- porté depuis lib/CMT2300a/cmt2300wrapper.cpp
// ---------------------------------------------------------------------------
bool HMSComponent::cmt_start_tx_(const uint8_t *buf, uint8_t len) {
  this->radio_->go_state(cmt2300a::GO_STBY, cmt2300a::STATE_STBY);
  this->radio_->clear_irq_flags();

  // Equivalent de CMT2300A_EnableWriteFifo() : ces deux bits doivent être positionnés
  // sans condition en FIFO fusionné -- on ne peut pas réutiliser radio_->fifo_write_enable()
  // qui ne positionne FIFO_RX_TX_SEL que si le flag interne is_fifo_merged_ de cmt2300a est
  // vrai, ce qui n'est jamais le cas ici puisque le FIFO_CTL est configuré directement par
  // init_radio_() sans passer par cmt2300a::set_merge_fifo_().
  uint8_t fifo_ctl = this->radio_->read_reg(cmt2300a::REG_CUS_FIFO_CTL);
  fifo_ctl |= cmt2300a::MASK_SPI_FIFO_RD_WR_SEL;
  fifo_ctl |= cmt2300a::MASK_FIFO_RX_TX_SEL;
  this->radio_->write_reg(cmt2300a::REG_CUS_FIFO_CTL, fifo_ctl);

  this->radio_->fifo_clear_tx();

  this->radio_->write_reg(cmt2300a::REG_CUS_PKT15, len);  // longueur Tx dynamique
  this->radio_->write_fifo(buf, len);

  uint8_t fifo_flag = this->radio_->read_reg(cmt2300a::REG_CUS_FIFO_FLAG);
  if (!(fifo_flag & 0x02 /* TX_FIFO_NMTY_FLG */)) {
    ESP_LOGW(TAG, "FIFO Tx vide après écriture -- abandon");
    return false;
  }

  if (!this->radio_->go_state(cmt2300a::GO_TX, cmt2300a::STATE_TX)) {
    return false;
  }

  // Ne bloque plus ici : process_tx_() (appelée à chaque tick de loop()) surveille
  // TX_DONE sans attente active, pour ne pas geler le reste d'ESPHome pendant l'émission.
  this->tx_sending_ = true;
  this->tx_start_ = millis();
  return true;
}

void HMSComponent::process_tx_() {
  if (!this->tx_sending_) return;

  bool done = (this->radio_->read_reg(cmt2300a::REG_CUS_INT_CLR1) & cmt2300a::MASK_TX_DONE_FLG) != 0;
  bool timed_out = millis() - this->tx_start_ > 95;
  if (!done && !timed_out) return;  // toujours en cours -- on repasse la main à ESPHome

  if (!done) {
    ESP_LOGW(TAG, "Timeout Tx (95ms)");
  } else {
    ESP_LOGV(TAG, "TX terminée (%ums)", millis() - this->tx_start_);
  }

  this->radio_->clear_irq_flags();
  this->radio_->go_state(cmt2300a::GO_SLEEP, cmt2300a::STATE_SLEEP);
  this->tx_sending_ = false;

  if (this->tx_restore_channel_) {
    this->switch_to_channel_(this->tx_restore_channel_value_);
    this->tx_restore_channel_ = false;
  }
  if (this->tx_release_lock_after_) {
    this->radio_->unlock_external(this);
    this->tx_release_lock_after_ = false;
  }
  this->cmt_start_listening_();
}

bool HMSComponent::cmt_start_listening_() {
  this->radio_->go_state(cmt2300a::GO_STBY, cmt2300a::STATE_STBY);
  this->radio_->clear_irq_flags();

  // Equivalent de CMT2300A_EnableReadFifo() -- même remarque que ci-dessus.
  uint8_t fifo_ctl = this->radio_->read_reg(cmt2300a::REG_CUS_FIFO_CTL);
  fifo_ctl &= ~cmt2300a::MASK_SPI_FIFO_RD_WR_SEL;
  fifo_ctl &= ~cmt2300a::MASK_FIFO_RX_TX_SEL;
  this->radio_->write_reg(cmt2300a::REG_CUS_FIFO_CTL, fifo_ctl);

  this->radio_->fifo_clear_rx();
  return this->radio_->go_state(cmt2300a::GO_RX, cmt2300a::STATE_RX);
}

bool HMSComponent::cmt_rx_packet_available_() {
  return (this->radio_->read_reg(cmt2300a::REG_CUS_INT_FLAG) & cmt2300a::MASK_PKT_OK_FLG) != 0;
}

uint8_t HMSComponent::cmt_read_dynamic_payload_(uint8_t *buf, uint8_t maxlen) {
  uint8_t len = 0;
  this->radio_->read_fifo(&len, 1);  // 1er octet FIFO = longueur du paquet
  if (len > maxlen) len = maxlen;
  this->radio_->read_fifo(buf, len);
  this->last_rssi_dbm_ = static_cast<int8_t>(static_cast<int>(this->radio_->read_reg(cmt2300a::REG_CUS_RSSI_DBM)) - 128);
  this->radio_->clear_irq_flags();
  return len;
}

// ---------------------------------------------------------------------------
// Construction de trames -- portées depuis commands/*.cpp
// ---------------------------------------------------------------------------
void HMSComponent::build_realtime_data_request_(uint8_t *out, uint8_t *out_len) {
  memset(out, 0, 27);
  out[0] = 0x15;
  serial_to_packet_id(&out[1], this->inverter_serial_);
  serial_to_packet_id(&out[5], this->dtu_serial_);
  out[9] = 0x80;
  out[10] = 0x0b;  // data type: RealTimeRunData
  out[11] = 0x00;

  time_t now = time(nullptr);
  out[12] = static_cast<uint8_t>(static_cast<uint32_t>(now) >> 24);
  out[13] = static_cast<uint8_t>(static_cast<uint32_t>(now) >> 16);
  out[14] = static_cast<uint8_t>(static_cast<uint32_t>(now) >> 8);
  out[15] = static_cast<uint8_t>(static_cast<uint32_t>(now));
  // out[16..23] = gap + password, laissés à 0

  uint16_t crc = crc16(&out[10], 14);
  out[24] = static_cast<uint8_t>(crc >> 8);
  out[25] = static_cast<uint8_t>(crc);

  out[26] = crc8(out, 26);
  *out_len = 27;
}

void HMSComponent::build_request_frame_(uint8_t frame_no, uint8_t *out, uint8_t *out_len) {
  memset(out, 0, 11);
  out[0] = 0x15;
  serial_to_packet_id(&out[1], this->inverter_serial_);
  serial_to_packet_id(&out[5], this->dtu_serial_);
  out[9] = frame_no | 0x80;
  out[10] = crc8(out, 10);
  *out_len = 11;
}

void HMSComponent::build_active_power_control_(float limit, PowerLimitType type, uint8_t *out, uint8_t *out_len) {
  memset(out, 0, 19);
  out[0] = 0x51;
  serial_to_packet_id(&out[1], this->inverter_serial_);
  serial_to_packet_id(&out[5], this->dtu_serial_);
  out[9] = 0x81;
  out[10] = 0x0b;  // sous-commande ActivePowerControl
  out[11] = 0x00;

  uint16_t l = static_cast<uint16_t>(limit * 10.0f);
  out[12] = static_cast<uint8_t>(l >> 8);
  out[13] = static_cast<uint8_t>(l);

  // Valeurs HMS (HmsActivePowerControl) : Absolu non-persistant = 0x0000, Relatif non-persistant = 0x0001
  uint16_t type_value = (type == POWER_RELATIVE) ? 0x0001 : 0x0000;
  out[14] = static_cast<uint8_t>(type_value >> 8);
  out[15] = static_cast<uint8_t>(type_value);

  uint16_t crc = crc16(&out[10], 6);
  out[16] = static_cast<uint8_t>(crc >> 8);
  out[17] = static_cast<uint8_t>(crc);

  out[18] = crc8(out, 18);
  *out_len = 19;
}

void HMSComponent::build_channel_change_(uint8_t channel, uint8_t *out, uint8_t *out_len) {
  memset(out, 0, 15);
  out[0] = 0x56;
  serial_to_packet_id(&out[1], this->inverter_serial_);
  serial_to_packet_id(&out[5], this->dtu_serial_);
  if (this->frequency_band_ == FrequencyBand::US_900) {
    out[9] = 0x03;
    out[10] = 0x17;
    out[11] = 0x3c;
  } else {
    out[9] = 0x02;
    out[10] = 0x15;
    out[11] = 0x21;
  }
  out[12] = channel;
  out[13] = 0x14;
  out[14] = crc8(out, 14);
  *out_len = 15;
}

// ---------------------------------------------------------------------------
// Réassemblage des fragments -- porté depuis inverters/InverterAbstract.cpp
// ---------------------------------------------------------------------------
void HMSComponent::clear_rx_fragment_buffer_() {
  for (auto &f : this->rx_fragments_) {
    f.wasReceived = false;
    f.len = 0;
  }
  this->rx_fragment_last_id_ = 0;
  this->rx_fragment_max_id_ = 0;
  this->rx_retransmit_count_ = 0;
}

void HMSComponent::add_rx_fragment_(const uint8_t *fragment, uint8_t len) {
  if (len < 11) return;
  if (len - 11 > 32) return;

  uint8_t fragment_count = fragment[9];
  uint8_t fragment_id = fragment_count & 0x7F;
  if (fragment_id == 0 || fragment_id >= MAX_RF_FRAGMENT_COUNT) return;

  fragment_t &slot = this->rx_fragments_[fragment_id - 1];
  memcpy(slot.fragment, &fragment[10], len - 11);
  slot.len = len - 11;
  slot.mainCmd = fragment[0];
  slot.wasReceived = true;

  if (fragment_id > this->rx_fragment_last_id_) this->rx_fragment_last_id_ = fragment_id;
  if (fragment_count & 0x80) this->rx_fragment_max_id_ = fragment_id;
}

uint8_t HMSComponent::verify_all_fragments_() {
  if (this->rx_fragment_last_id_ == 0) {
    if (this->send_count_ <= MAX_RESEND_COUNT) return FRAGMENT_ALL_MISSING_RESEND;
    return FRAGMENT_ALL_MISSING_TIMEOUT;
  }

  if (this->rx_fragment_max_id_ == 0) {
    if (this->rx_retransmit_count_++ < MAX_RETRANSMIT_COUNT) return this->rx_fragment_last_id_ + 1;
    return FRAGMENT_RETRANSMIT_TIMEOUT;
  }

  for (uint8_t i = 0; i < static_cast<uint8_t>(this->rx_fragment_max_id_ - 1); i++) {
    if (!this->rx_fragments_[i].wasReceived) {
      if (this->rx_retransmit_count_++ < MAX_RETRANSMIT_COUNT) return i + 1;
      return FRAGMENT_RETRANSMIT_TIMEOUT;
    }
  }

  bool ok;
  switch (this->pending_cmd_) {
    case CMD_REALTIME_DATA:
      ok = this->handle_realtime_response_();
      break;
    case CMD_ACTIVE_POWER_CONTROL: {
      ok = true;
      for (uint8_t i = 0; i < this->rx_fragment_max_id_; i++) {
        if (this->rx_fragments_[i].mainCmd != 0xD1) ok = false;  // 0x51 | 0x80
      }
      break;
    }
    default:
      ok = true;
      break;
  }
  return ok ? FRAGMENT_OK : FRAGMENT_HANDLE_ERROR;
}

bool HMSComponent::handle_realtime_response_() {
  uint16_t crc = 0xffff, crc_rcv = 0;
  uint8_t total_len = 0;

  for (uint8_t i = 0; i < this->rx_fragment_max_id_; i++) {
    if (this->rx_fragments_[i].mainCmd != 0x95) {  // 0x15 | 0x80
      ESP_LOGW(TAG, "Réponse RealTimeData : mainCmd inattendu (0x%02X)", this->rx_fragments_[i].mainCmd);
      return false;
    }
    total_len += this->rx_fragments_[i].len;

    if (i == this->rx_fragment_max_id_ - 1) {
      crc = crc16(this->rx_fragments_[i].fragment, this->rx_fragments_[i].len - 2, crc);
      crc_rcv = (static_cast<uint16_t>(this->rx_fragments_[i].fragment[this->rx_fragments_[i].len - 2]) << 8) |
                this->rx_fragments_[i].fragment[this->rx_fragments_[i].len - 1];
    } else {
      crc = crc16(this->rx_fragments_[i].fragment, this->rx_fragments_[i].len, crc);
    }
  }

  if (crc != crc_rcv) {
    ESP_LOGW(TAG, "CRC16 invalide sur la réponse RealTimeData");
    return false;
  }
  if (total_len < this->expected_byte_count_) {
    ESP_LOGW(TAG, "Réponse trop courte (%u/%u octets attendus)", total_len, this->expected_byte_count_);
    return false;
  }

  uint8_t offs = 0;
  memset(this->stats_buf_, 0, sizeof(this->stats_buf_));
  for (uint8_t i = 0; i < this->rx_fragment_max_id_; i++) {
    uint8_t l = this->rx_fragments_[i].len;
    if (offs + l > sizeof(this->stats_buf_)) break;
    memcpy(&this->stats_buf_[offs], this->rx_fragments_[i].fragment, l);
    offs += l;
  }

  this->has_valid_stats_ = true;
  this->rx_failure_count_ = 0;
  this->publish_reachable_();
  this->publish_sensors_();
  return true;
}

// ---------------------------------------------------------------------------
// Décodage des champs -- porté depuis parser/StatisticsParser.cpp
// ---------------------------------------------------------------------------
const byteAssign_t *HMSComponent::find_assignment_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const {
  for (uint8_t i = 0; i < this->byte_assignment_size_; i++) {
    const auto &a = this->byte_assignment_[i];
    if (a.type == type && a.ch == ch && a.fieldId == field) return &a;
  }
  return nullptr;
}

bool HMSComponent::has_field_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const {
  return this->find_assignment_(type, ch, field) != nullptr;
}

float HMSComponent::get_field_value_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const {
  const byteAssign_t *pos = this->find_assignment_(type, ch, field);
  if (pos == nullptr) return 0.0f;

  if (pos->div != CMD_CALC) {
    uint8_t ptr = pos->start;
    uint8_t end = ptr + pos->num;
    uint32_t val = 0;
    while (ptr != end) {
      val <<= 8;
      val |= this->stats_buf_[ptr];
      ptr++;
    }
    float result;
    if (pos->isSigned && pos->num == 2) {
      result = static_cast<float>(static_cast<int16_t>(val));
    } else if (pos->isSigned && pos->num == 4) {
      result = static_cast<float>(static_cast<int32_t>(val));
    } else {
      result = static_cast<float>(val);
    }
    return result / static_cast<float>(pos->div);
  }

  // Champs calculés -- portés depuis les fonctions calc*() de StatisticsParser.cpp
  switch (pos->start) {
    case CALC_TOTAL_YT: {
      float y = 0;
      for (uint8_t c = 0; c < this->dc_channel_count_; c++) y += this->get_field_value_(TYPE_DC, static_cast<ChannelNum_t>(c), FLD_YT);
      return y;
    }
    case CALC_TOTAL_YD: {
      float y = 0;
      for (uint8_t c = 0; c < this->dc_channel_count_; c++) y += this->get_field_value_(TYPE_DC, static_cast<ChannelNum_t>(c), FLD_YD);
      return y;
    }
    case CALC_TOTAL_PDC: {
      float p = 0;
      for (uint8_t c = 0; c < this->dc_channel_count_; c++) p += this->get_field_value_(TYPE_DC, static_cast<ChannelNum_t>(c), FLD_PDC);
      return p;
    }
    case CALC_TOTAL_EFF: {
      float ac = this->get_field_value_(TYPE_AC, CH0, FLD_PAC);
      float dc = 0;
      for (uint8_t c = 0; c < this->dc_channel_count_; c++) dc += this->get_field_value_(TYPE_DC, static_cast<ChannelNum_t>(c), FLD_PDC);
      return dc > 0 ? (ac / dc * 100.0f) : 0.0f;
    }
    default:
      return 0.0f;
  }
}

void HMSComponent::publish_sensors_() {
  for (uint8_t c = 0; c < this->dc_channel_count_; c++) {
    auto ch = static_cast<ChannelNum_t>(c);
    if (this->dc_power_[c] != nullptr) this->dc_power_[c]->publish_state(this->get_field_value_(TYPE_DC, ch, FLD_PDC));
    if (this->dc_current_[c] != nullptr) this->dc_current_[c]->publish_state(this->get_field_value_(TYPE_DC, ch, FLD_IDC));
    if (this->dc_voltage_[c] != nullptr) this->dc_voltage_[c]->publish_state(this->get_field_value_(TYPE_DC, ch, FLD_UDC));
    if (this->dc_energy_today_[c] != nullptr) this->dc_energy_today_[c]->publish_state(this->get_field_value_(TYPE_DC, ch, FLD_YD));
    if (this->dc_energy_total_[c] != nullptr) this->dc_energy_total_[c]->publish_state(this->get_field_value_(TYPE_DC, ch, FLD_YT));
  }

  float ac_power_value = this->get_field_value_(TYPE_AC, CH0, FLD_PAC);
  if (this->ac_voltage_ != nullptr) this->ac_voltage_->publish_state(this->get_field_value_(TYPE_AC, CH0, FLD_UAC));
  if (this->ac_current_ != nullptr) this->ac_current_->publish_state(this->get_field_value_(TYPE_AC, CH0, FLD_IAC));
  if (this->ac_power_ != nullptr) this->ac_power_->publish_state(ac_power_value);
  if (this->ac_frequency_ != nullptr) this->ac_frequency_->publish_state(this->get_field_value_(TYPE_AC, CH0, FLD_F));
  if (this->ac_power_factor_ != nullptr) this->ac_power_factor_->publish_state(this->get_field_value_(TYPE_AC, CH0, FLD_PF));
  if (this->ac_reactive_power_ != nullptr) this->ac_reactive_power_->publish_state(this->get_field_value_(TYPE_AC, CH0, FLD_Q));

  // isProducing() -- porté de InverterAbstract::isProducing() (totalAc > 0)
  if (this->producing_sensor_ != nullptr) this->producing_sensor_->publish_state(ac_power_value > 0.0f);

  if (this->inv_temperature_ != nullptr) this->inv_temperature_->publish_state(this->get_field_value_(TYPE_INV, CH0, FLD_T));
  if (this->inv_power_ != nullptr) this->inv_power_->publish_state(this->get_field_value_(TYPE_INV, CH0, FLD_PDC));
  if (this->inv_energy_today_ != nullptr) this->inv_energy_today_->publish_state(this->get_field_value_(TYPE_INV, CH0, FLD_YD));
  if (this->inv_energy_total_ != nullptr) this->inv_energy_total_->publish_state(this->get_field_value_(TYPE_INV, CH0, FLD_YT));
  if (this->inv_efficiency_ != nullptr) this->inv_efficiency_->publish_state(this->get_field_value_(TYPE_INV, CH0, FLD_EFF));
  if (this->rssi_sensor_ != nullptr) this->rssi_sensor_->publish_state(this->last_rssi_dbm_);
}

// isReachable() -- porté de InverterAbstract::isReachable() (rxFailureCount <= reachableThreshold)
void HMSComponent::publish_reachable_() {
  if (this->reachable_sensor_ != nullptr) {
    this->reachable_sensor_->publish_state(this->rx_failure_count_ <= REACHABLE_THRESHOLD);
  }
}

// ---------------------------------------------------------------------------
// Commandes de puissance -- exposées à la plateforme number
// ---------------------------------------------------------------------------
void HMSComponent::set_power_limit_percent(float percent) {
  this->power_limit_value_ = percent;
  this->power_limit_type_ = POWER_RELATIVE;
  this->power_limit_pending_ = true;
}

void HMSComponent::set_power_limit_absolute(float watts) {
  this->power_limit_value_ = watts;
  this->power_limit_type_ = POWER_ABSOLUTE;
  this->power_limit_pending_ = true;
}

// ---------------------------------------------------------------------------
// Machine à état Tx/Rx + cadence de polling -- portée depuis HoymilesRadio.cpp
// et Hoymiles.cpp (boucle principale)
// ---------------------------------------------------------------------------
void HMSComponent::start_command_(PendingCmd cmd, const uint8_t *payload, uint8_t len, uint32_t timeout_ms) {
  this->pending_cmd_ = cmd;
  memcpy(this->tx_payload_, payload, len);
  this->tx_payload_len_ = len;
  this->send_count_ = 0;
  this->send_current_command_();
  this->op_state_ = OP_WAIT_RESPONSE;
  this->cmd_deadline_ = millis() + timeout_ms;
}

void HMSComponent::send_current_command_() {
  this->send_count_++;
  ESP_LOGV(TAG, "send_current_command_ : tentative n°%u (cmd=%u, len=%u)", this->send_count_, this->pending_cmd_,
           this->tx_payload_len_);
  this->clear_rx_fragment_buffer_();
  if (!this->cmt_start_tx_(this->tx_payload_, this->tx_payload_len_)) {
    ESP_LOGW(TAG, "  cmt_start_tx_ a échoué immédiatement");
  }
  // process_tx_() relance l'écoute une fois TX_DONE (ou timeout) constaté, de façon
  // non bloquante -- voir loop().
}

void HMSComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HMS...");

  if (this->radio_ == nullptr) {
    ESP_LOGE(TAG, "Aucun radio cmt2300a associé");
    this->mark_failed();
    return;
  }
  if (this->radio_->is_failed()) {
    ESP_LOGE(TAG, "Le composant cmt2300a est en échec, HMS ne peut pas démarrer");
    this->mark_failed();
    return;
  }

  if (!this->decode_serial_()) {
    ESP_LOGE(TAG, "Numéro de série 0x%012llX non reconnu (préfixe HMS inconnu)",
              static_cast<unsigned long long>(this->inverter_serial_));
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "Modèle détecté : %s (%u canal/canaux DC)", this->type_name_.c_str(), this->dc_channel_count_);

  if (this->dtu_serial_ == 0) {
    this->dtu_serial_ = HMSComponent::generate_dtu_serial_();
  }
  ESP_LOGCONFIG(TAG, "DTU serial : 0x%012llX", static_cast<unsigned long long>(this->dtu_serial_));

  uint8_t band_variant = static_cast<uint8_t>(this->frequency_band_);
  if (this->radio_->is_external_radio_ready()) {
    // Une autre instance hms: partageant ce même cmt2300a: a déjà fait l'init radio
    // (bancs de registres, FIFO fusionné) -- on ne la refait pas, mais on vérifie
    // que la bande de fréquence demandée est bien la même, sinon la puce ne peut
    // physiquement pas servir les deux à la fois.
    if (this->radio_->get_external_radio_variant() != band_variant) {
      ESP_LOGE(TAG, "Bande de fréquence incohérente : une autre instance hms: a déjà initialisé "
                    "ce cmt2300a: sur une bande différente. Toutes les instances hms: partageant "
                    "le même cmt2300a_id doivent utiliser la même frequency_band.");
      this->mark_failed();
      return;
    }
    ESP_LOGD(TAG, "Radio déjà initialisée par une autre instance hms: -- réutilisation");
  } else {
    if (!this->init_radio_()) {
      ESP_LOGE(TAG, "Init radio Hoymiles échouée (bancs %s)",
                this->frequency_band_ == FrequencyBand::US_900 ? "900MHz" : "860MHz");
      this->mark_failed();
      return;
    }
    this->radio_->set_external_radio_variant(band_variant);
    this->radio_->set_external_radio_ready(true);
  }

  uint32_t default_freq = (this->frequency_band_ == FrequencyBand::US_900) ? 918000000UL : 865000000UL;
  this->switch_to_frequency_(default_freq);
  this->cmt_start_listening_();

  ESP_LOGCONFIG(TAG, "HMS prêt, écoute sur %.3f MHz (canal %u)",
                this->frequency_from_channel_(this->work_channel_) / 1.0e6, this->work_channel_);
  this->publish_reachable_();
}

void HMSComponent::loop() {
  if (this->is_failed() || this->radio_ == nullptr) return;

  // Une autre instance hms: (même cmt2300a: partagé) est en train d'utiliser la
  // radio -- on ne touche à aucun registre tant qu'elle n'a pas rendu la main.
  if (this->radio_->is_owned_by_other(this)) return;

  // 0. Emission Tx en cours : on ne fait rien d'autre tant qu'elle n'est pas
  // terminée (ou en timeout) -- le chip est en mode TX, pas RX, pendant ce temps.
  if (this->tx_sending_) {
    this->process_tx_();
    return;
  }

  // 1. Réception : un paquet est-il disponible dans le FIFO ?
  if (this->cmt_rx_packet_available_()) {
    uint8_t raw[33];
    uint8_t len = this->cmt_read_dynamic_payload_(raw, sizeof(raw));
    this->radio_->fifo_clear_rx();

    if (len >= 12 && len <= 32) {
      uint8_t crc = crc8(raw, len - 1);
      if (crc == raw[len - 1]) {
        uint8_t source_id[4];
        serial_to_packet_id(source_id, this->dtu_serial_);
        // Le CMT2300A ne filtre pas les paquets par adresse -- on le fait nous-mêmes,
        // comme HoymilesRadio_CMT::loop() (memcmp sur l'adresse source du fragment).
        if (memcmp(&raw[5], source_id, 4) == 0) {
          this->add_rx_fragment_(raw, len);
        }
      }
    }
  }

  // 2. Suivi de la commande en cours
  if (this->op_state_ == OP_WAIT_RESPONSE && millis() > this->cmd_deadline_) {
    uint8_t result = this->verify_all_fragments_();
    ESP_LOGV(TAG, "verify_all_fragments_ -> %u (millis=%u, deadline=%u, last_id=%u, max_id=%u)", result, millis(),
             this->cmd_deadline_, this->rx_fragment_last_id_, this->rx_fragment_max_id_);

    if (result == FRAGMENT_OK) {
      this->op_state_ = OP_IDLE;
      this->pending_cmd_ = CMD_NONE;
      this->radio_->unlock_external(this);
      this->cmt_start_listening_();
    } else if (result == FRAGMENT_ALL_MISSING_RESEND) {
      this->send_current_command_();
      this->cmd_deadline_ = millis() + 500;
    } else if (result >= 1 && result < MAX_RF_FRAGMENT_COUNT) {
      uint8_t out[16], out_len;
      this->build_request_frame_(result, out, &out_len);
      this->cmt_start_tx_(out, out_len);
      this->cmd_deadline_ = millis() + 500;
    } else {
      // Timeout définitif ou erreur de traitement
      this->rx_failure_count_++;
      ESP_LOGD(TAG, "Echec définitif du cycle de commande (résultat=%u) -- rx_failure_count_=%u", result,
               this->rx_failure_count_);
      this->publish_reachable_();
      this->op_state_ = OP_IDLE;
      this->pending_cmd_ = CMD_NONE;
      this->radio_->unlock_external(this);
      this->cmt_start_listening_();
    }
  }

  // 3. Cadence de polling (uniquement si le canal radio est libre)
  if (this->op_state_ == OP_IDLE && millis() - this->last_poll_ > this->poll_interval_ms_) {
    // Plusieurs hms: peuvent partager le même cmt2300a: -- on ne lance un nouvel
    // échange que si on parvient à prendre la main sur la radio. Sinon on retente
    // au prochain tick, sans décaler la fenêtre de poll (pas de mise à jour de
    // last_poll_ tant que le jeton n'est pas obtenu).
    if (!this->radio_->try_lock_external(this)) {
      return;
    }
    this->last_poll_ = millis();
    ESP_LOGV(TAG, "Cycle de polling (millis=%u, rx_failure_count_=%u)", millis(), this->rx_failure_count_);

    if (this->rx_failure_count_ > REACHABLE_THRESHOLD) {
      // Onduleur injoignable -> ChannelChangeCommand envoyée à la fréquence de boot,
      // comme HMS_Abstract::sendChangeChannelRequest() / HoymilesRadio_CMT::sendEsbPacket()
      ESP_LOGW(TAG, "Onduleur injoignable (%u échecs) -- tentative de ChannelChangeCommand", this->rx_failure_count_);
      uint8_t out[16], out_len;
      uint32_t boot_freq = (this->frequency_band_ == FrequencyBand::US_900) ? 915000000UL : 868000000UL;
      uint8_t saved_channel = this->work_channel_;
      this->switch_to_frequency_(boot_freq);
      this->build_channel_change_(saved_channel, out, &out_len);
      this->tx_restore_channel_ = true;
      this->tx_restore_channel_value_ = saved_channel;
      this->tx_release_lock_after_ = true;  // fire-and-forget -- rien n'attend de réponse
      this->cmt_start_tx_(out, out_len);
      // process_tx_() restaure le canal de travail, relâche le jeton et relance
      // l'écoute une fois la trame partie (ou en timeout), sans bloquer loop().
    } else if (this->power_limit_pending_) {
      uint8_t out[24], out_len;
      this->build_active_power_control_(this->power_limit_value_, this->power_limit_type_, out, &out_len);
      this->power_limit_pending_ = false;
      this->start_command_(CMD_ACTIVE_POWER_CONTROL, out, out_len, 2000);
    } else {
      uint8_t out[32], out_len;
      this->build_realtime_data_request_(out, &out_len);
      this->start_command_(CMD_REALTIME_DATA, out, out_len, 500);
    }
  }
}

void HMSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HMS:");
  ESP_LOGCONFIG(TAG, "  Modèle: %s", this->type_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Numéro de série onduleur: 0x%012llX", static_cast<unsigned long long>(this->inverter_serial_));
  ESP_LOGCONFIG(TAG, "  Numéro de série DTU: 0x%012llX", static_cast<unsigned long long>(this->dtu_serial_));
  ESP_LOGCONFIG(TAG, "  Bande de fréquence: %s", this->frequency_band_ == FrequencyBand::US_900 ? "900MHz (US/BR)" : "860MHz (EU)");
  ESP_LOGCONFIG(TAG, "  Intervalle de sondage: %ums", this->poll_interval_ms_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup a échoué");
  }
}

}  // namespace hms
}  // namespace esphome
