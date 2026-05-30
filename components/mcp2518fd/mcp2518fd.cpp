#include "mcp2518fd.h"
#include "esphome/core/log.h"

// ============================================================
// ESPHome native component  —  MCP2518FD
// CAN FD controller (SPI interface)
//
// Architecture:
//   - TXQ  (FIFO CH0)  used for transmit
//   - FIFO CH1          used for receive
//   - Filter 0          accept-all (SID/EID both 0, mask 0)
//
// SPI command format (MCP2518FD):
//   Byte 0:  [CMD(3:0) | ADDR(11:8)]
//   Byte 1:  ADDR(7:0)
//   Byte 2+: data (LSB first, 32-bit words in little-endian order)
// ============================================================

namespace esphome::mcp2518fd {

static const char *const TAG = "mcp2518fd";

// ============================================================
// SPI low-level primitives
// ============================================================

uint32_t MCP2518FD::read_sfr_(uint16_t addr) {
  uint8_t cmd[2];
  build_cmd_(addr, MCP2518FD_INSTRUCTION_READ, cmd);

  this->enable();
  this->transfer_byte(cmd[0]);
  this->transfer_byte(cmd[1]);
  uint32_t val = 0;
  val |= static_cast<uint32_t>(this->transfer_byte(0x00));
  val |= static_cast<uint32_t>(this->transfer_byte(0x00)) << 8;
  val |= static_cast<uint32_t>(this->transfer_byte(0x00)) << 16;
  val |= static_cast<uint32_t>(this->transfer_byte(0x00)) << 24;
  this->disable();
  return val;
}

void MCP2518FD::write_sfr_(uint16_t addr, uint32_t value) {
  uint8_t cmd[2];
  build_cmd_(addr, MCP2518FD_INSTRUCTION_WRITE, cmd);

  this->enable();
  this->transfer_byte(cmd[0]);
  this->transfer_byte(cmd[1]);
  this->transfer_byte(static_cast<uint8_t>(value));
  this->transfer_byte(static_cast<uint8_t>(value >> 8));
  this->transfer_byte(static_cast<uint8_t>(value >> 16));
  this->transfer_byte(static_cast<uint8_t>(value >> 24));
  this->disable();
}

uint8_t MCP2518FD::read_byte_(uint16_t addr) {
  return static_cast<uint8_t>(read_sfr_(addr & ~0x03u) >> ((addr & 0x03u) * 8));
}

void MCP2518FD::write_byte_(uint16_t addr, uint8_t value) {
  uint16_t word_addr = addr & ~0x03u;
  uint32_t shift = (addr & 0x03u) * 8;
  uint32_t reg = read_sfr_(word_addr);
  reg = (reg & ~(0xFFUL << shift)) | (static_cast<uint32_t>(value) << shift);
  write_sfr_(word_addr, reg);
}

void MCP2518FD::read_ram_(uint16_t addr, uint8_t *data, uint8_t n) {
  uint8_t cmd[2];
  build_cmd_(addr, MCP2518FD_INSTRUCTION_READ, cmd);

  this->enable();
  this->transfer_byte(cmd[0]);
  this->transfer_byte(cmd[1]);
  for (uint8_t i = 0; i < n; i++) {
    data[i] = this->transfer_byte(0x00);
  }
  this->disable();
}

void MCP2518FD::write_ram_(uint16_t addr, const uint8_t *data, uint8_t n) {
  uint8_t cmd[2];
  build_cmd_(addr, MCP2518FD_INSTRUCTION_WRITE, cmd);

  this->enable();
  this->transfer_byte(cmd[0]);
  this->transfer_byte(cmd[1]);
  for (uint8_t i = 0; i < n; i++) {
    this->transfer_byte(data[i]);
  }
  this->disable();
}

// ============================================================
// Reset
// ============================================================

canbus::Error MCP2518FD::reset_() {
  // The RESET instruction is 0x00 on both command bytes
  this->enable();
  this->transfer_byte(0x00);
  this->transfer_byte(0x00);
  this->disable();
  delay(5);
  ESP_LOGV(TAG, "reset_() complete");
  return canbus::ERROR_OK;
}

// ============================================================
// Mode change
// ============================================================

canbus::Error MCP2518FD::set_mode_(CanOperationMode mode) {
  uint32_t reg = read_sfr_(REG_CiCON);

  // Clear REQOP bits, set new mode
  reg &= ~CiCON_REQOP_MASK;
  reg |= (static_cast<uint32_t>(mode) << CiCON_REQOP_SHIFT) & CiCON_REQOP_MASK;
  write_sfr_(REG_CiCON, reg);

  // Poll OPMOD until it matches (with 10 ms timeout)
  uint32_t t_start = millis();
  while ((millis() - t_start) < 10) {
    uint32_t con = read_sfr_(REG_CiCON);
    uint8_t opmod = static_cast<uint8_t>((con & CiCON_OPMOD_MASK) >> CiCON_OPMOD_SHIFT);
    if (opmod == static_cast<uint8_t>(mode)) {
      return canbus::ERROR_OK;
    }
  }
  ESP_LOGE(TAG, "set_mode_: timeout waiting for mode %d", mode);
  return canbus::ERROR_FAIL;
}

// ============================================================
// Oscillator configuration
// ============================================================

canbus::Error MCP2518FD::configure_osc_() {
  // Determine desired oscillator / PLL configuration
  // MCP2518FD always runs its CAN core at 40 MHz (System Clock = 40 MHz)
  // Supported input oscillators: 40, 20, 10, 4 MHz (4 MHz uses 10× PLL)
  uint32_t osc_val = 0;

  switch (this->mcp_clock_) {
    case MCP_40MHZ:
      // No PLL, no dividers — direct 40 MHz input
      osc_val = 0x00000000UL;
      break;
    case MCP_20MHZ:
      // SCLKDIV=1 → SCLK = 40/2 = 20 MHz → but we need 40 MHz system clock
      // So use PLL: 20 MHz × 2 = 40 MHz is not how MCP2518FD works.
      // Actually with 20 MHz crystal, SCLKDIV=0 → SCLK=20 MHz, PLL not usable.
      // Run at 20 MHz (lower SPI throughput but simpler)
      osc_val = 0x00000000UL;  // OSC=20 MHz, no PLL, no division
      break;
    case MCP_10MHZ:
      osc_val = 0x00000000UL;
      break;
    case MCP_4MHZ:
      // Enable 10× PLL: 4 MHz × 10 = 40 MHz
      osc_val = OSC_PLLEN;
      break;
  }

  write_sfr_(REG_OSC, osc_val);

  // Wait for oscillator / PLL ready
  uint32_t t_start = millis();
  while ((millis() - t_start) < 10) {
    uint32_t status = read_sfr_(REG_OSC);
    bool pll_needed = (osc_val & OSC_PLLEN) != 0;
    bool osc_rdy = (status & OSC_OSCRDY) != 0;
    bool pll_rdy = !pll_needed || ((status & OSC_PLLRDY) != 0);
    if (osc_rdy && pll_rdy) {
      ESP_LOGV(TAG, "OSC ready, OSC reg=0x%08X", status);
      return canbus::ERROR_OK;
    }
  }
  ESP_LOGE(TAG, "configure_osc_: oscillator/PLL not ready");
  return canbus::ERROR_FAIL;
}

// ============================================================
// Bit timing
// ============================================================

// Pre-computed NBT register values for common speeds
// Format: {BRP, TSEG1, TSEG2, SJW}  → CiNBTCFG = BRP<<24 | TSEG1<<16 | TSEG2<<8 | SJW
// Table entries indexed by [CanClock][CanSpeed]
//
// Formula: Tbit = (1 + TSEG1 + TSEG2) × TQ  where TQ = (BRP+1)/Fosc
// All values use BRP=0 unless noted.

struct NbtEntry {
  uint8_t brp;
  uint8_t tseg1;
  uint8_t tseg2;
  uint8_t sjw;
};

// clang-format off
// Fosc = 40 MHz (MCP_40MHZ)
static const NbtEntry NBT_40MHZ[] = {
  // CAN_5KBPS    BRP=99, TSEG1=31, TSEG2=8, SJW=8 → 5 kbps @ 40 MHz
  {99, 31, 8, 8},
  // CAN_10KBPS
  {49, 31, 8, 8},
  // CAN_20KBPS
  {24, 31, 8, 8},
  // CAN_31K25BPS
  {15, 31, 8, 8},
  // CAN_33KBPS
  {14, 31, 8, 8},
  // CAN_40KBPS
  {12, 31, 6, 6},
  // CAN_50KBPS
  { 9, 31, 8, 8},
  // CAN_80KBPS
  { 6, 31, 8, 8},
  // CAN_100KBPS
  { 4, 31, 8, 8},
  // CAN_125KBPS   BRP=1, TSEG1=126, TSEG2=33, SJW=33 → 125 kbps @ 40 MHz
  { 1,126,33,33},
  // CAN_200KBPS
  { 1, 74,25,25},
  // CAN_250KBPS   BRP=0, TSEG1=126, TSEG2=33, SJW=33 → 250 kbps
  { 0,126,33,33},
  // CAN_500KBPS   BRP=0, TSEG1=62, TSEG2=17, SJW=17
  { 0, 62,17,17},
  // CAN_1000KBPS  BRP=0, TSEG1=30, TSEG2=9, SJW=9
  { 0, 30, 9, 9},
};

// Fosc = 20 MHz
static const NbtEntry NBT_20MHZ[] = {
  {49, 31, 8, 8},  // 5K
  {24, 31, 8, 8},  // 10K
  {12, 31, 6, 6},  // 20K
  { 7, 31, 8, 8},  // 31.25K
  { 7, 29, 8, 8},  // 33K
  { 6, 31, 6, 6},  // 40K
  { 4, 31, 8, 8},  // 50K
  { 3, 31, 6, 6},  // 80K
  { 2, 31, 6, 6},  // 100K
  { 0,126,33,33},  // 125K
  { 0, 74,25,25},  // 200K
  { 0, 62,17,17},  // 250K
  { 0, 30, 9, 9},  // 500K
  { 0, 14, 5, 5},  // 1M
};

// Fosc = 10 MHz
static const NbtEntry NBT_10MHZ[] = {
  {24, 31, 8, 8},  // 5K
  {12, 31, 6, 6},  // 10K
  { 6, 31, 6, 6},  // 20K
  { 3, 31, 8, 8},  // 31.25K
  { 3, 29, 8, 8},  // 33K
  { 3, 23, 7, 7},  // 40K
  { 2, 31, 6, 6},  // 50K
  { 1, 31, 6, 6},  // 80K
  { 1, 23, 6, 6},  // 100K
  { 0, 62,17,17},  // 125K
  { 0, 36,13,13},  // 200K
  { 0, 30, 9, 9},  // 250K
  { 0, 14, 5, 5},  // 500K
  { 0,  6, 3, 3},  // 1M
};

// Fosc = 40 MHz via PLL (4 MHz × 10) — same as 40 MHz table
static const NbtEntry NBT_4MHZ[] = {
  {99, 31, 8, 8},  // 5K
  {49, 31, 8, 8},  // 10K
  {24, 31, 8, 8},  // 20K
  {15, 31, 8, 8},  // 31.25K
  {14, 31, 8, 8},  // 33K
  {12, 31, 6, 6},  // 40K
  { 9, 31, 8, 8},  // 50K
  { 6, 31, 8, 8},  // 80K
  { 4, 31, 8, 8},  // 100K
  { 1,126,33,33},  // 125K
  { 1, 74,25,25},  // 200K
  { 0,126,33,33},  // 250K
  { 0, 62,17,17},  // 500K
  { 0, 30, 9, 9},  // 1M
};
// clang-format on

// Speed enum to index map (same order as canbus::CanSpeed)
static const int SPEED_IDX_COUNT = 14;
static const canbus::CanSpeed SPEED_ORDER[SPEED_IDX_COUNT] = {
    canbus::CAN_5KBPS,   canbus::CAN_10KBPS,  canbus::CAN_20KBPS,
    canbus::CAN_31K25BPS,canbus::CAN_33KBPS,  canbus::CAN_40KBPS,
    canbus::CAN_50KBPS,  canbus::CAN_80KBPS,  canbus::CAN_100KBPS,
    canbus::CAN_125KBPS, canbus::CAN_200KBPS,  canbus::CAN_250KBPS,
    canbus::CAN_500KBPS, canbus::CAN_1000KBPS,
};

static int speed_index_(canbus::CanSpeed s) {
  for (int i = 0; i < SPEED_IDX_COUNT; i++) {
    if (SPEED_ORDER[i] == s) return i;
  }
  return -1;
}

bool MCP2518FD::calc_nbt_(canbus::CanSpeed speed, CanClock clk, uint32_t &nbtcfg) const {
  int idx = speed_index_(speed);
  if (idx < 0) return false;
  const NbtEntry *tbl = nullptr;
  switch (clk) {
    case MCP_40MHZ: tbl = NBT_40MHZ; break;
    case MCP_20MHZ: tbl = NBT_20MHZ; break;
    case MCP_10MHZ: tbl = NBT_10MHZ; break;
    case MCP_4MHZ:  tbl = NBT_4MHZ;  break;
    default: return false;
  }
  const NbtEntry &e = tbl[idx];
  nbtcfg = (static_cast<uint32_t>(e.brp)   << 24) |
           (static_cast<uint32_t>(e.tseg1)  << 16) |
           (static_cast<uint32_t>(e.tseg2)  <<  8) |
            static_cast<uint32_t>(e.sjw);
  return true;
}

bool MCP2518FD::calc_dbt_(canbus::CanSpeed speed, CanClock clk,
                           uint32_t &dbtcfg, uint32_t &tdcval) const {
  // Data bit time for CAN FD — simplified: use same pre-computed values
  // as nominal but with smaller segment counts where feasible.
  // For simplicity we re-use the same table scaled, capped at DBT limits.
  int idx = speed_index_(speed);
  if (idx < 0) return false;
  const NbtEntry *tbl = nullptr;
  switch (clk) {
    case MCP_40MHZ: tbl = NBT_40MHZ; break;
    case MCP_20MHZ: tbl = NBT_20MHZ; break;
    case MCP_10MHZ: tbl = NBT_10MHZ; break;
    case MCP_4MHZ:  tbl = NBT_4MHZ;  break;
    default: return false;
  }
  const NbtEntry &e = tbl[idx];
  // CiDBTCFG uses 4-bit SJW, 5-bit TSEG1, 4-bit TSEG2, 8-bit BRP
  uint8_t dsjw   = e.sjw   < 15 ? e.sjw   : 15;
  uint8_t dtseg2 = e.tseg2 < 15 ? e.tseg2 : 15;
  uint8_t dtseg1 = e.tseg1 < 31 ? e.tseg1 : 31;
  dbtcfg = (static_cast<uint32_t>(e.brp)  << 24) |
           (static_cast<uint32_t>(dtseg1) << 16) |
           (static_cast<uint32_t>(dtseg2) <<  8) |
            static_cast<uint32_t>(dsjw);
  // TDC: TDCV=0, TDCO = propagation delay ≈ TSEG1+1 (Auto mode)
  tdcval = (0x02UL << 8) | static_cast<uint32_t>(dtseg1 + 1);  // TDCMOD=2 (auto), TDCO
  return true;
}

canbus::Error MCP2518FD::configure_bit_timing_() {
  uint32_t nbtcfg = 0;
  if (!calc_nbt_(this->bit_rate_, this->mcp_clock_, nbtcfg)) {
    ESP_LOGE(TAG, "configure_bit_timing_: unsupported speed/clock combination");
    return canbus::ERROR_FAIL;
  }
  write_sfr_(REG_CiNBTCFG, nbtcfg);
  ESP_LOGV(TAG, "CiNBTCFG = 0x%08X", nbtcfg);

  if (this->canfd_enabled_) {
    uint32_t dbtcfg = 0, tdcval = 0;
    if (!calc_dbt_(this->data_rate_, this->mcp_clock_, dbtcfg, tdcval)) {
      ESP_LOGE(TAG, "configure_bit_timing_: unsupported FD data speed");
      return canbus::ERROR_FAIL;
    }
    write_sfr_(REG_CiDBTCFG, dbtcfg);
    write_sfr_(REG_CiTDC,    tdcval);
    ESP_LOGV(TAG, "CiDBTCFG = 0x%08X  CiTDC = 0x%08X", dbtcfg, tdcval);
  }
  return canbus::ERROR_OK;
}

// ============================================================
// FIFO configuration
// ============================================================

canbus::Error MCP2518FD::configure_fifos_() {
  // --- TX Queue (CH0) --------------------------------------------------
  // 4 messages deep, 8 payload bytes (or 64 for FD), priority 31
  uint8_t  tx_plsize = this->canfd_enabled_ ? 7 : 0;  // 7=64 bytes, 0=8 bytes
  uint32_t txqcon = 0;
  txqcon |= (31UL <<  0);  // TxPriority = 31 (highest)
  txqcon |= ( 0UL <<  5);  // TxAttempts = 0 (unlimited)
  txqcon |= ( 3UL <<  8);  // FifoSize = 4 messages (= 3+1)
  txqcon |= (static_cast<uint32_t>(tx_plsize) << 11);  // PayLoadSize
  txqcon |= FIFOCON_TXEN;  // mark as TX
  write_sfr_(REG_CiTXQCON, txqcon);

  // --- RX FIFO CH1 -----------------------------------------------------
  // 8 messages deep, 8 payload bytes (or 64 for FD)
  uint8_t rx_plsize = this->canfd_enabled_ ? 7 : 0;
  uint32_t rxcon = 0;
  rxcon |= ( 7UL <<  8);  // FifoSize = 8 messages
  rxcon |= (static_cast<uint32_t>(rx_plsize) << 11);  // PayLoadSize
  // TXEN=0 → RX FIFO
  uint16_t rxfifo_addr = REG_CiFIFOCON + CIFIFO_OFFSET * 1;  // CH1
  write_sfr_(rxfifo_addr, rxcon);

  return canbus::ERROR_OK;
}

// ============================================================
// Filter configuration  — accept all frames
// ============================================================

canbus::Error MCP2518FD::configure_filters_() {
  // Filter 0: accept all standard + extended frames
  // CiFLTCON byte 0: enable filter 0, link to FIFO CH1
  uint16_t fltcon_addr = REG_CiFLTCON;
  uint8_t  fltcon_byte = (1 << 7) | 1;  // FLTEN0=1, F0BP=1 (point to FIFO CH1)
  write_byte_(fltcon_addr, fltcon_byte);

  // CiFLTOBJ0: ID=0, EXIDE=0 → matches both standard and extended (mask=0)
  write_sfr_(REG_CiFLTOBJ, 0x00000000UL);

  // CiMASK0: all zeros → all frames pass through
  write_sfr_(REG_CiMASK, 0x00000000UL);

  return canbus::ERROR_OK;
}

// ============================================================
// setup_internal
// ============================================================

bool MCP2518FD::setup_internal() {
  this->spi_setup();

  if (reset_() != canbus::ERROR_OK) return false;

  // Must be in Configuration mode to change bit timing / filters
  if (set_mode_(CAN_CONFIGURATION_MODE) != canbus::ERROR_OK) return false;

  if (configure_osc_() != canbus::ERROR_OK) return false;

  if (configure_bit_timing_() != canbus::ERROR_OK) return false;

  if (configure_fifos_() != canbus::ERROR_OK) return false;

  if (configure_filters_() != canbus::ERROR_OK) return false;

  // CiCON: enable TXQ, ISO CRC, restrict re-tx in FD mode
  uint32_t con = read_sfr_(REG_CiCON);
  con |= CiCON_TXQEN;
  if (this->canfd_enabled_) {
    con |= CiCON_ISOCRCEN;  // ISO 11898-1:2015 CRC
  } else {
    con |= CiCON_BRSDIS;    // disable BRS in classic mode
  }
  write_sfr_(REG_CiCON, con);

  // Enable RX and TX-attempt interrupts
  uint32_t inten = INT_RXIF | INT_CERRIF | INT_RXOVIF;
  write_byte_(REG_CiINTENABLE, static_cast<uint8_t>(inten));

  if (set_mode_(static_cast<CanOperationMode>(this->mcp_mode_)) != canbus::ERROR_OK)
    return false;

  uint32_t err = read_sfr_(REG_CiBDIAG0);
  ESP_LOGD(TAG, "mcp2518fd setup done, CiBDIAG0=0x%08X", err);
  return true;
}

// ============================================================
// TX helpers
// ============================================================

uint16_t MCP2518FD::tx_fifo_addr_() {
  // CiTXQUA holds the 16-bit user address of the next writable TX object
  return static_cast<uint16_t>(read_sfr_(REG_CiTXQUA));
}

canbus::Error MCP2518FD::send_message_txq_(struct canbus::CanFrame *frame) {
  uint8_t dlc = frame->can_data_length_code;
  if (!this->canfd_enabled_ && dlc > canbus::CAN_MAX_DATA_LENGTH) {
    return canbus::ERROR_FAILTX;
  }
  if (dlc > CAN_FD_MAX_DATA_LENGTH) {
    return canbus::ERROR_FAILTX;
  }

  // Check TX queue not full
  uint32_t txqsta = read_sfr_(REG_CiTXQSTA);
  if ((txqsta & TXQSTA_TXQNF) == 0) {
    return canbus::ERROR_ALLTXBUSY;
  }

  // Get write address from UA register
  uint16_t ua = tx_fifo_addr_();
  uint16_t ram_addr = RAM_ADDR_START + ua;

  // Build 8-byte message object header (no timestamp)
  uint8_t obj[8 + CAN_FD_MAX_DATA_LENGTH];
  memset(obj, 0, sizeof(obj));

  // Word 0: ID
  uint32_t id_word = 0;
  if (frame->use_extended_id) {
    uint32_t eid = frame->can_id & 0x3FFFFUL;
    uint32_t sid = (frame->can_id >> 18) & 0x7FFU;
    id_word = eid | (sid << MSGOBJ_SID_SHIFT);
  } else {
    id_word = (static_cast<uint32_t>(frame->can_id) & 0x7FFU) << MSGOBJ_SID_SHIFT;
  }
  obj[0] = id_word;
  obj[1] = id_word >> 8;
  obj[2] = id_word >> 16;
  obj[3] = id_word >> 24;

  // Word 1: CTRL
  uint32_t ctrl = dlc & MSGOBJ_DLC_MASK;
  if (frame->use_extended_id)                  ctrl |= MSGOBJ_IDE;
  if (frame->remote_transmission_request)      ctrl |= MSGOBJ_RTR;
  if (this->canfd_enabled_ && dlc > 8)         ctrl |= MSGOBJ_FDF | MSGOBJ_BRS;
  obj[4] = ctrl;
  obj[5] = ctrl >> 8;
  obj[6] = ctrl >> 16;
  obj[7] = ctrl >> 24;

  // Data
  memcpy(&obj[8], frame->data, dlc);

  uint8_t total = 8 + dlc;
  write_ram_(ram_addr, obj, total);

  // Set UINC + TXREQ to request transmission
  uint32_t txqcon = read_sfr_(REG_CiTXQCON);
  txqcon |= TXQCON_UINC | TXQCON_TXREQ;
  write_sfr_(REG_CiTXQCON, txqcon);

  return canbus::ERROR_OK;
}

canbus::Error MCP2518FD::send_message(struct canbus::CanFrame *frame) {
  return send_message_txq_(frame);
}

// ============================================================
// RX helpers
// ============================================================

uint16_t MCP2518FD::rx_fifo_addr_() {
  uint16_t rxua_addr = REG_CiFIFOUA + CIFIFO_OFFSET * 1;  // CH1
  return static_cast<uint16_t>(read_sfr_(rxua_addr));
}

bool MCP2518FD::check_receive_() {
  uint16_t rxsta_addr = REG_CiFIFOSTA + CIFIFO_OFFSET * 1;
  uint32_t sta = read_sfr_(rxsta_addr);
  return (sta & FIFOSTA_RXNOTEMPTY) != 0;
}

canbus::Error MCP2518FD::read_message_fifo_(struct canbus::CanFrame *frame) {
  uint16_t rxsta_addr = REG_CiFIFOSTA  + CIFIFO_OFFSET * 1;
  uint16_t rxcon_addr = REG_CiFIFOCON  + CIFIFO_OFFSET * 1;

  uint32_t sta = read_sfr_(rxsta_addr);
  if ((sta & FIFOSTA_RXNOTEMPTY) == 0) {
    return canbus::ERROR_NOMSG;
  }

  // Check overflow
  if (sta & FIFOSTA_RXOVIF) {
    ESP_LOGD(TAG, "RX FIFO overflow – clearing flag");
    // Clear RXOVIF in CiRXOVIF (bit 1 for CH1)
    uint32_t ovif = read_sfr_(REG_CiRXOVIF);
    write_sfr_(REG_CiRXOVIF, ovif & ~(1UL << 1));
  }

  uint16_t ua = rx_fifo_addr_();
  uint16_t ram_addr = RAM_ADDR_START + ua;

  // Read header (8 bytes)
  uint8_t hdr[8];
  read_ram_(ram_addr, hdr, 8);

  uint32_t id_word = hdr[0] | (hdr[1] << 8) | (hdr[2] << 16) | (hdr[3] << 24);
  uint32_t ctrl    = hdr[4] | (hdr[5] << 8) | (hdr[6] << 16) | (hdr[7] << 24);

  bool extended = (ctrl & MSGOBJ_IDE) != 0;
  bool rtr      = (ctrl & MSGOBJ_RTR) != 0;
  uint8_t dlc   = static_cast<uint8_t>(ctrl & MSGOBJ_DLC_MASK);
  uint8_t nbytes = dlc_to_bytes_(dlc);

  if (nbytes > CAN_FD_MAX_DATA_LENGTH) {
    // Corrupt message, skip
    goto uinc;
  }

  frame->use_extended_id            = extended;
  frame->remote_transmission_request = rtr;
  frame->can_data_length_code        = nbytes;

  if (extended) {
    uint32_t sid = (id_word & MSGOBJ_SID_MASK) >> MSGOBJ_SID_SHIFT;
    uint32_t eid =  id_word & MSGOBJ_EID_MASK;
    frame->can_id = (sid << 18) | eid;
  } else {
    frame->can_id = (id_word >> MSGOBJ_SID_SHIFT) & 0x7FFU;
  }

  if (nbytes > 0) {
    read_ram_(ram_addr + 8, frame->data, nbytes);
  }

uinc:
  // Increment FIFO head (UINC)
  {
    uint32_t rxcon = read_sfr_(rxcon_addr);
    rxcon |= FIFOCON_UINC;
    write_sfr_(rxcon_addr, rxcon);
  }

  return canbus::ERROR_OK;
}

canbus::Error MCP2518FD::read_message(struct canbus::CanFrame *frame) {
  return read_message_fifo_(frame);
}

// ============================================================
// Utility
// ============================================================

uint8_t MCP2518FD::dlc_to_bytes_(uint8_t dlc) {
  if (dlc < 16) return DLC_TO_BYTES[dlc];
  return 0;
}

uint8_t MCP2518FD::bytes_to_dlc_(uint8_t len) {
  for (uint8_t d = 0; d < 16; d++) {
    if (DLC_TO_BYTES[d] >= len) return d;
  }
  return 15;  // 64 bytes
}

bool MCP2518FD::check_error_() {
  uint32_t trec = read_sfr_(REG_CiTREC);
  return (trec & 0x00600000UL) != 0;  // TXBO or TXBP bits
}

}  // namespace esphome::mcp2518fd
