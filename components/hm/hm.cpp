#include "hm.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <ctime>

namespace esphome {
namespace hm {

static const char *const TAG = "hm";

// ---------------------------------------------------------------------------
// CRC -- identiques à hms (crc.cpp d'OpenDTU)
// ---------------------------------------------------------------------------
static uint8_t crc8(const uint8_t *buf, uint8_t len) {
  uint8_t crc = 0x00;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= buf[i];
    for (uint8_t b = 0; b < 8; b++) crc = (crc << 1) ^ ((crc & 0x80) ? 0x01 : 0x00);
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

static const uint8_t FRAGMENT_ALL_MISSING_RESEND = 255;
static const uint8_t FRAGMENT_ALL_MISSING_TIMEOUT = 254;
static const uint8_t FRAGMENT_RETRANSMIT_TIMEOUT = 253;
static const uint8_t FRAGMENT_HANDLE_ERROR = 252;
static const uint8_t FRAGMENT_OK = 0;
static const uint8_t MAX_RESEND_COUNT = 4;
static const uint8_t MAX_RETRANSMIT_COUNT = 5;

// ---------------------------------------------------------------------------
// Tables d'octets -- portées 1:1 depuis inverters/HM_1CH.cpp, HM_2CH.cpp, HM_4CH.cpp
// (FLD_IRR non porté, comme pour hms -- nécessite une config de puissance crête
// par string non exposée en v1)
// ---------------------------------------------------------------------------
static const byteAssign_t HM_1CH_TABLE[] = {
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

static const byteAssign_t HM_2CH_TABLE[] = {
    {TYPE_DC, CH0, FLD_UDC, 2, 2, 10, false, 1},
    {TYPE_DC, CH0, FLD_IDC, 4, 2, 100, false, 2},
    {TYPE_DC, CH0, FLD_PDC, 6, 2, 10, false, 1},
    {TYPE_DC, CH0, FLD_YD, 22, 2, 1, false, 0},
    {TYPE_DC, CH0, FLD_YT, 14, 4, 1000, false, 3},

    {TYPE_DC, CH1, FLD_UDC, 8, 2, 10, false, 1},
    {TYPE_DC, CH1, FLD_IDC, 10, 2, 100, false, 2},
    {TYPE_DC, CH1, FLD_PDC, 12, 2, 10, false, 1},
    {TYPE_DC, CH1, FLD_YD, 24, 2, 1, false, 0},
    {TYPE_DC, CH1, FLD_YT, 18, 4, 1000, false, 3},

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

static const byteAssign_t HM_4CH_TABLE[] = {
    {TYPE_DC, CH0, FLD_UDC, 2, 2, 10, false, 1},
    {TYPE_DC, CH0, FLD_IDC, 4, 2, 100, false, 2},
    {TYPE_DC, CH0, FLD_PDC, 8, 2, 10, false, 1},
    {TYPE_DC, CH0, FLD_YD, 20, 2, 1, false, 0},
    {TYPE_DC, CH0, FLD_YT, 12, 4, 1000, false, 3},

    {TYPE_DC, CH1, FLD_UDC, CALC_CH_UDC, CH0, CMD_CALC, false, 1},  // == tension CH0
    {TYPE_DC, CH1, FLD_IDC, 6, 2, 100, false, 2},
    {TYPE_DC, CH1, FLD_PDC, 10, 2, 10, false, 1},
    {TYPE_DC, CH1, FLD_YD, 22, 2, 1, false, 0},
    {TYPE_DC, CH1, FLD_YT, 16, 4, 1000, false, 3},

    {TYPE_DC, CH2, FLD_UDC, 24, 2, 10, false, 1},
    {TYPE_DC, CH2, FLD_IDC, 26, 2, 100, false, 2},
    {TYPE_DC, CH2, FLD_PDC, 30, 2, 10, false, 1},
    {TYPE_DC, CH2, FLD_YD, 42, 2, 1, false, 0},
    {TYPE_DC, CH2, FLD_YT, 34, 4, 1000, false, 3},

    {TYPE_DC, CH3, FLD_UDC, CALC_CH_UDC, CH2, CMD_CALC, false, 1},  // == tension CH2
    {TYPE_DC, CH3, FLD_IDC, 28, 2, 100, false, 2},
    {TYPE_DC, CH3, FLD_PDC, 32, 2, 10, false, 1},
    {TYPE_DC, CH3, FLD_YD, 44, 2, 1, false, 0},
    {TYPE_DC, CH3, FLD_YT, 38, 4, 1000, false, 3},

    {TYPE_AC, CH0, FLD_UAC, 46, 2, 10, false, 1},
    {TYPE_AC, CH0, FLD_IAC, 54, 2, 100, false, 2},
    {TYPE_AC, CH0, FLD_PAC, 50, 2, 10, false, 1},
    {TYPE_AC, CH0, FLD_Q, 52, 2, 10, true, 1},
    {TYPE_AC, CH0, FLD_F, 48, 2, 100, false, 2},
    {TYPE_AC, CH0, FLD_PF, 56, 2, 1000, false, 3},

    {TYPE_INV, CH0, FLD_T, 58, 2, 10, true, 1},
    {TYPE_INV, CH0, FLD_EVT_LOG, 60, 2, 1, false, 0},

    {TYPE_INV, CH0, FLD_YD, CALC_TOTAL_YD, 0, CMD_CALC, false, 0},
    {TYPE_INV, CH0, FLD_YT, CALC_TOTAL_YT, 0, CMD_CALC, false, 3},
    {TYPE_INV, CH0, FLD_PDC, CALC_TOTAL_PDC, 0, CMD_CALC, false, 1},
    {TYPE_INV, CH0, FLD_EFF, CALC_TOTAL_EFF, 0, CMD_CALC, false, 3},
};

// ---------------------------------------------------------------------------
// Adressage radio -- porté depuis HoymilesRadio::convertSerialToRadioId()
// (HoymilesRadio.cpp, base commune CMT/NRF) : 5 octets, 0x01 suivi des 4 octets
// de poids faible du numéro de série -- même dérivation que serial_to_packet_id
// côté hms, avec un octet 0x01 en préfixe (adresse de pipe nRF24 complète).
// ---------------------------------------------------------------------------
static void serial_to_radio_address(uint8_t out5[5], uint64_t serial) {
  out5[0] = 0x01;
  out5[1] = static_cast<uint8_t>(serial >> 24);
  out5[2] = static_cast<uint8_t>(serial >> 16);
  out5[3] = static_cast<uint8_t>(serial >> 8);
  out5[4] = static_cast<uint8_t>(serial >> 0);
}

// Même dérivation, mais seulement les 4 octets utilisés dans l'entête des trames
// (adressage cible/source à 32 bits, comme CommandAbstract::convertSerialToPacketId).
static void serial_to_packet_id(uint8_t out4[4], uint64_t serial) {
  out4[0] = static_cast<uint8_t>(serial >> 24);
  out4[1] = static_cast<uint8_t>(serial >> 16);
  out4[2] = static_cast<uint8_t>(serial >> 8);
  out4[3] = static_cast<uint8_t>(serial >> 0);
}

// ---------------------------------------------------------------------------
// Décodage du numéro de série -- porté depuis isValidSerial() de HM_1CH.cpp,
// HM_2CH.cpp, HM_4CH.cpp. Contrairement à hms, ce n'est PAS une simple
// comparaison de préfixe : formule bit à bit + cas particuliers, portée
// littéralement (pas de simplification).
// ---------------------------------------------------------------------------
static bool hm_1ch_valid(uint64_t serial) {
  uint8_t p0 = static_cast<uint8_t>(serial >> 40);
  uint8_t p1 = static_cast<uint8_t>(serial >> 32);
  if (static_cast<uint8_t>(((static_cast<uint16_t>(p0) << 8 | p1) >> 4) & 0xff) == 0x12) return true;
  if (((p1 & 0xf0) == 0x10 || (p1 & 0xf0) == 0x20) && ((p0 == 0x10 && p1 == 0x22) || (p0 == 0x11 && p1 == 0x21)))
    return true;
  return false;
}

static bool hm_2ch_valid(uint64_t serial) {
  uint8_t p0 = static_cast<uint8_t>(serial >> 40);
  uint8_t p1 = static_cast<uint8_t>(serial >> 32);
  if (static_cast<uint8_t>(((static_cast<uint16_t>(p0) << 8 | p1) >> 4) & 0xff) == 0x14) return true;
  if (((p1 & 0xf0) == 0x30 || (p1 & 0xf0) == 0x40) && ((p0 == 0x10 && p1 == 0x42) || (p0 == 0x11 && p1 == 0x41)))
    return true;
  return false;
}

static bool hm_4ch_valid(uint64_t serial) {
  uint8_t p0 = static_cast<uint8_t>(serial >> 40);
  uint8_t p1 = static_cast<uint8_t>(serial >> 32);
  if (static_cast<uint8_t>(((static_cast<uint16_t>(p0) << 8 | p1) >> 4) & 0xff) == 0x16) return true;
  if (((p1 & 0xf0) == 0x50 || (p1 & 0xf0) == 0x60) && ((p0 == 0x10 && p1 == 0x62) || (p0 == 0x11 && p1 == 0x61)))
    return true;
  return false;
}

bool HMComponent::decode_serial_() {
  if (hm_1ch_valid(this->inverter_serial_)) {
    this->dc_channel_count_ = 1;
    this->byte_assignment_ = HM_1CH_TABLE;
    this->byte_assignment_size_ = sizeof(HM_1CH_TABLE) / sizeof(HM_1CH_TABLE[0]);
    this->type_name_ = "HM-300/350/400-1T";
  } else if (hm_2ch_valid(this->inverter_serial_)) {
    this->dc_channel_count_ = 2;
    this->byte_assignment_ = HM_2CH_TABLE;
    this->byte_assignment_size_ = sizeof(HM_2CH_TABLE) / sizeof(HM_2CH_TABLE[0]);
    this->type_name_ = "HM-600/700/800-2T";
  } else if (hm_4ch_valid(this->inverter_serial_)) {
    this->dc_channel_count_ = 4;
    this->byte_assignment_ = HM_4CH_TABLE;
    this->byte_assignment_size_ = sizeof(HM_4CH_TABLE) / sizeof(HM_4CH_TABLE[0]);
    this->type_name_ = "HM-1000/1200/1500-4T";
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

// Identique à hms (Utils::generateDtuSerial(), dérivé de la MAC ESP32)
uint64_t HMComponent::generate_dtu_serial_() {
  uint8_t mac[6];
  get_mac_address_raw(mac);
  uint32_t chip_id =
      (static_cast<uint32_t>(mac[0])) | (static_cast<uint32_t>(mac[1]) << 8) | (static_cast<uint32_t>(mac[2]) << 16);

  uint64_t dtu_id = 0;
  dtu_id |= 0x199900000000ULL;
  dtu_id |= 0x80000000ULL;
  dtu_id |= 0x0100000ULL;
  for (uint8_t i = 0; i < 5; i++) {
    dtu_id |= static_cast<uint64_t>(chip_id % 10) << (i * 4);
    chip_id /= 10;
  }
  return dtu_id;
}

// ---------------------------------------------------------------------------
// Init radio -- porté depuis HoymilesRadio_NRF::init(). Ces réglages sont
// imposés par le protocole Hoymiles NRF (pas configurables par l'utilisateur
// via le nrf24l01: générique, contrairement à un usage "classique" du composant).
// ---------------------------------------------------------------------------
bool HMComponent::init_radio_() {
  // 250 kbps : RF_DR_LOW=1 (bit5), RF_DR_HIGH=0 (bit3) -- porté de
  // HoymilesRadio_NRF::init() -> _radio->setDataRate(RF24_250KBPS)
  uint8_t rf_setup = this->radio_->read_register(nrf24l01::REG_RF_SETUP);
  rf_setup &= ~0b00101000;
  rf_setup |= 0b00100000;
  this->radio_->write_register(nrf24l01::REG_RF_SETUP, rf_setup);

  // CRC 16 bits (EN_CRC=1, CRCO=1) -- setCRCLength(RF24_CRC_16)
  uint8_t cfg = this->radio_->read_register(nrf24l01::REG_CONFIG);
  cfg |= (nrf24l01::MASK_EN_CRC | nrf24l01::MASK_CRCO);
  this->radio_->write_register(nrf24l01::REG_CONFIG, cfg);

  // Largeur d'adresse 5 octets -- setAddressWidth(5) : SETUP_AW = width-2
  this->radio_->write_register(nrf24l01::REG_SETUP_AW, 0x03);

  // Payloads dynamiques -- enableDynamicPayloads()
  this->radio_->write_register(nrf24l01::REG_FEATURE, nrf24l01::FEATURE_EN_DPL);
  this->radio_->write_register(nrf24l01::REG_DYNPD, 0x3F);

  // Retries désactivés au repos, activés ponctuellement pendant Tx (voir cmt_start_tx_)
  this->radio_->set_retries_reg(0, 0);

  this->open_reading_pipe_for_dtu_();
  this->radio_->start_listening();
  return true;
}

void HMComponent::open_reading_pipe_for_dtu_() {
  uint8_t addr[5];
  serial_to_radio_address(addr, this->dtu_serial_);
  this->radio_->open_reading_pipe(1, addr);
}

void HMComponent::open_writing_pipe_for_inverter_() {
  uint8_t addr[5];
  serial_to_radio_address(addr, this->inverter_serial_);
  this->radio_->open_writing_pipe(addr);
}

uint8_t HMComponent::next_rx_channel_() {
  if (++this->rx_ch_idx_ >= sizeof(this->rx_ch_list_)) this->rx_ch_idx_ = 0;
  return this->rx_ch_list_[this->rx_ch_idx_];
}

uint8_t HMComponent::next_tx_channel_() {
  if (++this->tx_ch_idx_ >= sizeof(this->tx_ch_list_)) this->tx_ch_idx_ = 0;
  return this->tx_ch_list_[this->tx_ch_idx_];
}

void HMComponent::switch_rx_channel_() {
  this->radio_->stop_listening();
  this->radio_->set_channel_reg(this->next_rx_channel_());
  this->radio_->start_listening();
}

// ---------------------------------------------------------------------------
// Emission non bloquante -- porté de HoymilesRadio_NRF::sendEsbPacket(), avec
// le même principe de scrutation étalée sur plusieurs ticks de loop() que hms
// (voir process_tx_()) plutôt que le blocage synchrone de RF24::write().
// ---------------------------------------------------------------------------
bool HMComponent::cmt_start_tx_(const uint8_t *buf, uint8_t len) {
  this->radio_->stop_listening();
  this->radio_->set_channel_reg(this->next_tx_channel_());
  this->open_writing_pipe_for_inverter_();
  this->radio_->set_retries_reg(3, 15);

  this->radio_->flush_tx();
  this->radio_->write_payload(buf, len);
  this->radio_->get_ce_pin()->digital_write(true);
  delayMicroseconds(15);  // impulsion minimale pour déclencher la Tx
  this->radio_->get_ce_pin()->digital_write(false);

  this->tx_sending_ = true;
  this->tx_start_ = millis();
  return true;
}

void HMComponent::process_tx_() {
  if (!this->tx_sending_) return;

  uint8_t status = this->radio_->get_status();
  bool done = (status & (nrf24l01::STATUS_TX_DS | nrf24l01::STATUS_MAX_RT)) != 0;
  bool timed_out = millis() - this->tx_start_ > 50;
  if (!done && !timed_out) return;

  if (status & nrf24l01::STATUS_MAX_RT) {
    ESP_LOGW(TAG, "Envoi échoué : accusé de réception jamais reçu (MAX_RT)");
    this->radio_->flush_tx();
  } else if (timed_out && !done) {
    ESP_LOGW(TAG, "Timeout Tx");
    this->radio_->flush_tx();
  }
  this->radio_->write_register(nrf24l01::REG_STATUS, nrf24l01::STATUS_TX_DS | nrf24l01::STATUS_MAX_RT);
  this->tx_sending_ = false;

  this->radio_->set_retries_reg(0, 0);
  this->radio_->stop_listening();
  this->open_reading_pipe_for_dtu_();
  this->radio_->set_channel_reg(this->next_rx_channel_());
  this->radio_->start_listening();
}

// ---------------------------------------------------------------------------
// Construction de trames -- identiques hms (même format Hoymiles), sauf les
// valeurs de type persistant/non-persistant du contrôle de puissance (table
// HmActivePowerControl, différente de HmsActivePowerControl -- vérifié dans
// ActivePowerControlCommand.cpp).
// ---------------------------------------------------------------------------
void HMComponent::build_realtime_data_request_(uint8_t *out, uint8_t *out_len) {
  memset(out, 0, 27);
  out[0] = 0x15;
  serial_to_packet_id(&out[1], this->inverter_serial_);
  serial_to_packet_id(&out[5], this->dtu_serial_);
  out[9] = 0x80;
  out[10] = 0x0b;
  out[11] = 0x00;

  time_t now = time(nullptr);
  out[12] = static_cast<uint8_t>(static_cast<uint32_t>(now) >> 24);
  out[13] = static_cast<uint8_t>(static_cast<uint32_t>(now) >> 16);
  out[14] = static_cast<uint8_t>(static_cast<uint32_t>(now) >> 8);
  out[15] = static_cast<uint8_t>(static_cast<uint32_t>(now));

  uint16_t crc = crc16(&out[10], 14);
  out[24] = static_cast<uint8_t>(crc >> 8);
  out[25] = static_cast<uint8_t>(crc);
  out[26] = crc8(out, 26);
  *out_len = 27;
}

void HMComponent::build_request_frame_(uint8_t frame_no, uint8_t *out, uint8_t *out_len) {
  memset(out, 0, 11);
  out[0] = 0x15;
  serial_to_packet_id(&out[1], this->inverter_serial_);
  serial_to_packet_id(&out[5], this->dtu_serial_);
  out[9] = frame_no | 0x80;
  out[10] = crc8(out, 10);
  *out_len = 11;
}

void HMComponent::build_active_power_control_(float limit, PowerLimitType type, bool persistent, uint8_t *out,
                                               uint8_t *out_len) {
  memset(out, 0, 19);
  out[0] = 0x51;
  serial_to_packet_id(&out[1], this->inverter_serial_);
  serial_to_packet_id(&out[5], this->dtu_serial_);
  out[9] = 0x81;
  out[10] = 0x0b;
  out[11] = 0x00;

  uint16_t l = static_cast<uint16_t>(limit * 10.0f);
  out[12] = static_cast<uint8_t>(l >> 8);
  out[13] = static_cast<uint8_t>(l);

  // Table HmActivePowerControl (ActivePowerControlCommand.cpp) -- différente de
  // HmsActivePowerControl utilisée par hms :
  //   AbsolutNonPersistent=0x0000, RelativNonPersistent=0x0001,
  //   AbsolutPersistent=0x0100, RelativPersistent=0x0101
  uint16_t type_value;
  if (type == POWER_RELATIVE) {
    type_value = persistent ? 0x0101 : 0x0001;
  } else {
    type_value = persistent ? 0x0100 : 0x0000;
  }
  out[14] = static_cast<uint8_t>(type_value >> 8);
  out[15] = static_cast<uint8_t>(type_value);

  uint16_t crc = crc16(&out[10], 6);
  out[16] = static_cast<uint8_t>(crc >> 8);
  out[17] = static_cast<uint8_t>(crc);
  out[18] = crc8(out, 18);
  *out_len = 19;
}

// ---------------------------------------------------------------------------
// Réassemblage des fragments -- identique à hms (InverterAbstract.cpp)
// ---------------------------------------------------------------------------
void HMComponent::clear_rx_fragment_buffer_() {
  for (auto &f : this->rx_fragments_) {
    f.wasReceived = false;
    f.len = 0;
  }
  this->rx_fragment_last_id_ = 0;
  this->rx_fragment_max_id_ = 0;
  this->rx_retransmit_count_ = 0;
}

void HMComponent::add_rx_fragment_(const uint8_t *fragment, uint8_t len) {
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

uint8_t HMComponent::verify_all_fragments_() {
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

bool HMComponent::handle_realtime_response_() {
  uint16_t crc = 0xffff, crc_rcv = 0;
  uint8_t total_len = 0;

  for (uint8_t i = 0; i < this->rx_fragment_max_id_; i++) {
    if (this->rx_fragments_[i].mainCmd != 0x95) {
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
// Décodage des champs -- identique à hms, plus le cas CALC_CH_UDC
// ---------------------------------------------------------------------------
const byteAssign_t *HMComponent::find_assignment_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const {
  for (uint8_t i = 0; i < this->byte_assignment_size_; i++) {
    const auto &a = this->byte_assignment_[i];
    if (a.type == type && a.ch == ch && a.fieldId == field) return &a;
  }
  return nullptr;
}

float HMComponent::get_field_value_(ChannelType_t type, ChannelNum_t ch, FieldId_t field) const {
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
    case CALC_CH_UDC:
      // arg (pos->num) = canal source dont on recopie la tension -- porté de
      // StatisticsParser::calcChUdc()
      return this->get_field_value_(TYPE_DC, static_cast<ChannelNum_t>(pos->num), FLD_UDC);
    default:
      return 0.0f;
  }
}

void HMComponent::publish_sensors_() {
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
}

// isReachable() -- porté de InverterAbstract::isReachable() (rxFailureCount <= reachableThreshold)
void HMComponent::publish_reachable_() {
  if (this->reachable_sensor_ != nullptr) {
    this->reachable_sensor_->publish_state(this->rx_failure_count_ <= REACHABLE_THRESHOLD);
  }
}

// ---------------------------------------------------------------------------
// Commandes de puissance
// ---------------------------------------------------------------------------
void HMComponent::set_power_limit_percent(float percent) {
  this->power_limit_value_ = percent;
  this->power_limit_type_ = POWER_RELATIVE;
  this->power_limit_persistent_ = false;
  this->power_limit_pending_ = true;
}

void HMComponent::set_power_limit_absolute(float watts) {
  this->power_limit_value_ = watts;
  this->power_limit_type_ = POWER_ABSOLUTE;
  this->power_limit_persistent_ = false;
  this->power_limit_pending_ = true;
}

void HMComponent::set_power_limit_percent_persistent(float percent) {
  ESP_LOGI(TAG, "Limite persistante demandée : %.1f%% (écriture EEPROM côté onduleur)", percent);
  this->power_limit_value_ = percent;
  this->power_limit_type_ = POWER_RELATIVE;
  this->power_limit_persistent_ = true;
  this->power_limit_pending_ = true;
}

// ---------------------------------------------------------------------------
// send_current_command_ / start_command_ -- identiques à hms
// ---------------------------------------------------------------------------
void HMComponent::send_current_command_() {
  this->send_count_++;
  ESP_LOGV(TAG, "send_current_command_ : tentative n°%u (cmd=%u, len=%u)", this->send_count_, this->pending_cmd_,
           this->tx_payload_len_);
  this->clear_rx_fragment_buffer_();
  this->cmt_start_tx_(this->tx_payload_, this->tx_payload_len_);
}

void HMComponent::start_command_(PendingCmd cmd, const uint8_t *payload, uint8_t len, uint32_t timeout_ms) {
  this->pending_cmd_ = cmd;
  memcpy(this->tx_payload_, payload, len);
  this->tx_payload_len_ = len;
  this->send_count_ = 0;
  this->send_current_command_();
  this->op_state_ = OP_WAIT_RESPONSE;
  this->cmd_deadline_ = millis() + timeout_ms;
}

// ---------------------------------------------------------------------------
// ESPHome : setup / loop / dump_config
// ---------------------------------------------------------------------------
void HMComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HM...");

  if (this->radio_ == nullptr) {
    ESP_LOGE(TAG, "Aucun radio nrf24l01 associé");
    this->mark_failed();
    return;
  }
  if (this->radio_->is_failed()) {
    ESP_LOGE(TAG, "Le composant nrf24l01 est en échec, HM ne peut pas démarrer");
    this->mark_failed();
    return;
  }

  if (!this->decode_serial_()) {
    ESP_LOGE(TAG, "Numéro de série 0x%012llX non reconnu (préfixe HM inconnu)",
              static_cast<unsigned long long>(this->inverter_serial_));
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "Modèle détecté : %s (%u canal/canaux DC)", this->type_name_.c_str(), this->dc_channel_count_);

  if (this->dtu_serial_ == 0) {
    this->dtu_serial_ = HMComponent::generate_dtu_serial_();
  }
  ESP_LOGCONFIG(TAG, "DTU serial : 0x%012llX", static_cast<unsigned long long>(this->dtu_serial_));

  if (!this->init_radio_()) {
    ESP_LOGE(TAG, "Init radio Hoymiles NRF échouée");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "HM prêt, hop de canal actif (%u/%u/%u/%u/%u)", this->rx_ch_list_[0], this->rx_ch_list_[1],
                this->rx_ch_list_[2], this->rx_ch_list_[3], this->rx_ch_list_[4]);
  this->publish_reachable_();
}

void HMComponent::loop() {
  if (this->is_failed() || this->radio_ == nullptr) return;

  // 0. Emission Tx en cours -- non bloquant, comme hms
  if (this->tx_sending_) {
    this->process_tx_();
    return;
  }

  // 1. Hop de canal RX -- toutes les 4ms, porté de HoymilesRadio_NRF::loop()
  // (EVERY_N_MILLIS(4) { switchRxCh(); })
  if (millis() - this->last_rx_switch_ms_ >= 4) {
    this->last_rx_switch_ms_ = millis();
    this->switch_rx_channel_();
  }

  // 2. Réception -- uniquement pertinent quand on attend une réponse (voir la
  // note d'optimisation apportée à hms : le protocole est requête/réponse pur)
  if (this->op_state_ == OP_WAIT_RESPONSE && this->radio_->rx_available()) {
    uint8_t raw[33];
    uint8_t len = this->radio_->read_payload(raw, sizeof(raw));
    this->radio_->write_register(nrf24l01::REG_STATUS, nrf24l01::STATUS_RX_DR);

    if (len >= 12 && len <= 32) {
      uint8_t crc = crc8(raw, len - 1);
      if (crc == raw[len - 1]) {
        this->add_rx_fragment_(raw, len);
      }
    }
  }

  // 3. Suivi de la commande en cours
  if (this->op_state_ == OP_WAIT_RESPONSE && millis() > this->cmd_deadline_) {
    uint8_t result = this->verify_all_fragments_();

    if (result == FRAGMENT_OK) {
      this->op_state_ = OP_IDLE;
      this->pending_cmd_ = CMD_NONE;
    } else if (result == FRAGMENT_ALL_MISSING_RESEND) {
      this->send_current_command_();
      this->cmd_deadline_ = millis() + 500;
    } else if (result >= 1 && result < MAX_RF_FRAGMENT_COUNT) {
      uint8_t out[16], out_len;
      this->build_request_frame_(result, out, &out_len);
      this->cmt_start_tx_(out, out_len);
      this->cmd_deadline_ = millis() + 500;
    } else {
      this->rx_failure_count_++;
      ESP_LOGD(TAG, "Echec définitif du cycle de commande (résultat=%u) -- rx_failure_count_=%u", result,
               this->rx_failure_count_);
      this->publish_reachable_();
      this->op_state_ = OP_IDLE;
      this->pending_cmd_ = CMD_NONE;
    }
  }

  // 4. Cadence de polling
  if (this->op_state_ == OP_IDLE && millis() - this->last_poll_ > this->poll_interval_ms_) {
    this->last_poll_ = millis();

    if (this->power_limit_pending_) {
      uint8_t out[24], out_len;
      this->build_active_power_control_(this->power_limit_value_, this->power_limit_type_,
                                         this->power_limit_persistent_, out, &out_len);
      this->power_limit_pending_ = false;
      this->power_limit_persistent_ = false;
      this->start_command_(CMD_ACTIVE_POWER_CONTROL, out, out_len, 2000);
    } else {
      uint8_t out[32], out_len;
      this->build_realtime_data_request_(out, &out_len);
      this->start_command_(CMD_REALTIME_DATA, out, out_len, 500);
    }
  }
}

void HMComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HM:");
  ESP_LOGCONFIG(TAG, "  Modèle: %s", this->type_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Numéro de série onduleur: 0x%012llX", static_cast<unsigned long long>(this->inverter_serial_));
  ESP_LOGCONFIG(TAG, "  Numéro de série DTU: 0x%012llX", static_cast<unsigned long long>(this->dtu_serial_));
  ESP_LOGCONFIG(TAG, "  Intervalle de sondage: %ums", this->poll_interval_ms_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup a échoué");
  }
}

}  // namespace hm
}  // namespace esphome
