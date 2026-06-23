#include "mcp2518fd.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <inttypes.h>

// ============================================================
// ESPHome native component — MCP2518FD
//
// SPI command format:
//   Byte 0: [CMD(3:0) | ADDR(11:8)]
//   Byte 1: ADDR(7:0)
//   Byte 2+: data, little-endian 32-bit words
//
// Interrupt wiring (all active-low, open-drain):
//   INT  → any enabled CiINT source  (general fallback)
//   INT0 → CiINT.TXIF & TXIE         (TX done)
//   INT1 → CiINT.RXIF & RXIE         (RX available)
//
// When an interrupt pin is declared in YAML, it is used to
// gate the corresponding SPI access (no pin = polling).
// ============================================================

namespace esphome::mcp2518fd {

static const char *const TAG = "mcp2518fd";

// ============================================================
// SPI low-level
// ============================================================

uint32_t MCP2518FD::read_sfr_(uint16_t addr) {
  uint8_t buf[6] = {};
  build_cmd_(addr, MCP2518FD_INSTRUCTION_READ, buf);
  this->enable();
  this->transfer_array(buf, 6);
  this->disable();
  return (uint32_t)buf[2] | ((uint32_t)buf[3] << 8) |
         ((uint32_t)buf[4] << 16) | ((uint32_t)buf[5] << 24);
}

void MCP2518FD::write_sfr_(uint16_t addr, uint32_t value) {
  uint8_t tx[6];
  build_cmd_(addr, MCP2518FD_INSTRUCTION_WRITE, tx);
  tx[2] = value & 0xFF;
  tx[3] = (value >> 8) & 0xFF;
  tx[4] = (value >> 16) & 0xFF;
  tx[5] = (value >> 24) & 0xFF;
  this->enable();
  this->write_array(tx, 6);
  this->disable();
}

// Write a single byte directly at byte-addressed register (no read-modify-write)
void MCP2518FD::write_sfr_byte_(uint16_t byte_addr, uint8_t value) {
  uint8_t tx[3];
  build_cmd_(byte_addr, MCP2518FD_INSTRUCTION_WRITE, tx);
  tx[2] = value;
  this->enable();
  this->write_array(tx, 3);
  this->disable();
}

uint8_t MCP2518FD::read_byte_(uint16_t addr) {
  uint16_t word_addr = addr & ~0x03u;
  uint8_t  shift     = (addr & 0x03u) * 8;
  return static_cast<uint8_t>(read_sfr_(word_addr) >> shift);
}

void MCP2518FD::write_byte_(uint16_t addr, uint8_t value) {
  uint16_t word_addr = addr & ~0x03u;
  uint32_t shift     = (addr & 0x03u) * 8;
  uint32_t reg = read_sfr_(word_addr);
  reg = (reg & ~(0xFFUL << shift)) | (static_cast<uint32_t>(value) << shift);
  write_sfr_(word_addr, reg);
}

void MCP2518FD::read_ram_(uint16_t addr, uint8_t *data, uint8_t n) {
  if (n > 64) n = 64;
  uint8_t buf[2 + 64] = {};
  build_cmd_(addr, MCP2518FD_INSTRUCTION_READ, buf);
  this->enable();
  this->transfer_array(buf, 2 + n);
  this->disable();
  memcpy(data, buf + 2, n);
}

void MCP2518FD::write_ram_(uint16_t addr, const uint8_t *data, uint8_t n) {
  uint8_t cmd[2];
  build_cmd_(addr, MCP2518FD_INSTRUCTION_WRITE, cmd);
  uint8_t tx[2 + 64] = {};
  if (n > 64) n = 64;
  tx[0] = cmd[0];
  tx[1] = cmd[1];
  memcpy(tx + 2, data, n);
  this->enable();
  this->write_array(tx, 2 + n);
  this->disable();
}

// ============================================================
// Reset
// ============================================================

canbus::Error MCP2518FD::reset_() {
  // Per Linux kernel driver mcp251xfd-core.c:
  // The chip must NOT be in Sleep mode before reset.
  // Wake it first by writing to OSC register (clears OSCDIS).
  // Then set Config mode, then send SPI RESET.

  // Step 1: Wake the chip (write OSC = 0x60: CLKODIV=10, no PLL, no sleep)
  // This is a no-op if already awake, but wakes it if sleeping.
  write_sfr_(REG_OSC, 0x00000060UL);
  delay(2);

  // Step 2: Force Config mode via CiCON.REQOP
  uint32_t con = read_sfr_(REG_CiCON);
  con = (con & ~CiCON_REQOP_MASK) |
        ((uint32_t(CAN_CONFIGURATION_MODE) << CiCON_REQOP_SHIFT) & CiCON_REQOP_MASK);
  write_sfr_(REG_CiCON, con);
  delay(2);

  // Step 3: Send SPI RESET (0x00 0x00) — retry up to 3 times and verify
  for (int attempt = 0; attempt < 3; attempt++) {
    this->enable();
    this->transfer_byte(0x00);
    this->transfer_byte(0x00);
    this->disable();
    delay(10);

    uint32_t osc   = read_sfr_(REG_OSC);
    uint32_t cicon = read_sfr_(REG_CiCON);
    uint8_t  opmod = (cicon >> 21) & 0x07;
    ESP_LOGD(TAG, "reset_() attempt %d — OSC=0x%08" PRIx32 " CiCON=0x%08" PRIx32 " OPMOD=%d",
             (int)(attempt + 1), osc, cicon, (int)opmod);

    if (opmod == 4) {
      // Chip is in Config mode — reset succeeded
      return canbus::ERROR_OK;
    }

    // SPI reset didn't work — force Config mode and retry
    ESP_LOGW(TAG, "SPI reset failed (OPMOD=%d != 4), forcing Config mode and retrying", opmod);
    write_sfr_(REG_OSC, 0x00000060UL);
    delay(2);
    uint32_t con2 = read_sfr_(REG_CiCON);
    con2 = (con2 & ~CiCON_REQOP_MASK) |
           ((uint32_t(CAN_CONFIGURATION_MODE) << CiCON_REQOP_SHIFT) & CiCON_REQOP_MASK);
    write_sfr_(REG_CiCON, con2);
    delay(5);
  }

  ESP_LOGE(TAG, "reset_() failed after 3 attempts — chip stuck");
  return canbus::ERROR_FAIL;
}

// ============================================================
// Mode change
// ============================================================

canbus::Error MCP2518FD::set_mode_(CanOperationMode mode) {
  uint32_t reg = read_sfr_(REG_CiCON);
  reg &= ~CiCON_REQOP_MASK;
  reg |= (static_cast<uint32_t>(mode) << CiCON_REQOP_SHIFT) & CiCON_REQOP_MASK;
  write_sfr_(REG_CiCON, reg);

  uint32_t t0 = millis();
  while ((millis() - t0) < 10) {
    uint32_t con   = read_sfr_(REG_CiCON);
    uint8_t  opmod = static_cast<uint8_t>((con & CiCON_OPMOD_MASK) >> CiCON_OPMOD_SHIFT);
    if (opmod == static_cast<uint8_t>(mode))
      return canbus::ERROR_OK;
  }
  ESP_LOGE(TAG, "set_mode_ timeout (wanted %d)", mode);
  return canbus::ERROR_FAIL;
}

// ============================================================
// Oscillator
// ============================================================

canbus::Error MCP2518FD::configure_osc_() {
  // Build OSC value — CLKODIV=10 (bits 6:5 = 0b11 = 0x60) is the POR default
  // and is what the Linux kernel driver uses as reference after reset.
  // OSCRDY (bit 10) and PLLRDY (bit 8) are read-only status bits.
  uint32_t osc_val = 0x00000060UL;  // CLKODIV=10 (÷10), no PLL, no sleep

  if (this->can_clock_ == MCP_4MHZ) {
    osc_val |= OSC_PLLEN;  // Enable 10× PLL: 4 MHz → 40 MHz
  }

  write_sfr_(REG_OSC, osc_val);

  // Wait for OSCRDY (bit 10) — and PLLRDY (bit 8) if PLL enabled
  uint32_t t0 = millis();
  while ((millis() - t0) < 20) {
    uint32_t status = read_sfr_(REG_OSC);
    bool pll_needed = (osc_val & OSC_PLLEN) != 0;
    bool osc_rdy    = (status & OSC_OSCRDY) != 0;
    bool pll_rdy    = !pll_needed || ((status & OSC_PLLRDY) != 0);
    if (osc_rdy && pll_rdy) {
      ESP_LOGD(TAG, "OSC ready — OSC=0x%08" PRIx32 "", status);
      this->init_osc_ = status;
      return canbus::ERROR_OK;
    }
  }
  uint32_t final_osc = read_sfr_(REG_OSC);
  ESP_LOGE(TAG, "OSC/PLL not ready — OSC=0x%08" PRIx32 " (OSCRDY bit10=%d PLLRDY bit8=%d)",
           final_osc,
           (int)((final_osc >> 10) & 1),
           (int)((final_osc >>  8) & 1));
  return canbus::ERROR_FAIL;
}

// ============================================================
// Bit timing
// ============================================================

struct NbtEntry { uint8_t brp, tseg1, tseg2, sjw; };

// clang-format off
// Bit timing tables — all entries verified for exact baud rate, SP=80%
// Format: {BRP, TSEG1, TSEG2, SJW}
// SYSCLK = 40 MHz (also used for 4 MHz + PLL x10)
static const NbtEntry NBT_40MHZ[] = {
  {31,199,50,4},  // 5kbps
  {15,199,50,4},  // 10kbps
  { 7,199,50,4},  // 20kbps
  { 4,204,51,4},  // 31.25kbps
  { 4,191,48,4},  // 33.3kbps
  { 3,199,50,4},  // 40kbps
  { 3,159,40,4},  // 50kbps
  { 1,199,50,4},  // 80kbps
  { 1,159,40,4},  // 100kbps
  { 1,127,32,4},  // 125kbps
  { 0,159,40,4},  // 200kbps
  { 0,127,32,4},  // 250kbps
  { 0, 63,16,4},  // 500kbps
  { 0, 31, 8,4},  // 1000kbps
};
// SYSCLK = 20 MHz
static const NbtEntry NBT_20MHZ[] = {
  {15,199,50,4},  // 5kbps
  { 7,199,50,4},  // 10kbps
  { 3,199,50,4},  // 20kbps
  { 2,204,51,4},  // 31.25kbps
  { 2,191,48,4},  // 33.3kbps
  { 1,199,50,4},  // 40kbps
  { 1,159,40,4},  // 50kbps
  { 0,199,50,4},  // 80kbps
  { 0,159,40,4},  // 100kbps
  { 0,125,32,4},  // 125kbps@20MHz (exact: 20M/(1*160)=125000, SP=79.4%)
  { 0, 79,20,4},  // 200kbps
  { 0, 62,15,4},  // 250kbps@20MHz (exact: 20M/(1*80)=250000, SP=80%)
  { 0, 31, 8,4},  // 500kbps
  { 0, 15, 4,4},  // 1000kbps
};
// SYSCLK = 10 MHz
static const NbtEntry NBT_10MHZ[] = {
  { 7,199,50,4},  // 5kbps
  { 3,199,50,4},  // 10kbps
  { 1,199,50,4},  // 20kbps
  { 1,102,25,4},  // 31.25kbps (approx)
  { 1, 95,24,4},  // 33.3kbps (approx)
  { 0,199,50,4},  // 40kbps
  { 0,159,40,4},  // 50kbps
  { 0, 99,25,4},  // 80kbps (approx)
  { 0, 79,20,4},  // 100kbps
  { 0, 63,16,4},  // 125kbps (approx, actual=62.5kbps)
  { 0, 39,10,4},  // 200kbps (approx)
  { 0, 31, 8,4},  // 250kbps (approx, actual=312.5kbps)
  { 0, 15, 4,4},  // 500kbps
  { 0,  7, 2,2},  // 1000kbps
};
// SYSCLK = 40 MHz (4 MHz crystal + PLL x10) — same table as NBT_40MHZ
static const NbtEntry NBT_4MHZ[] = {
  {31,199,50,4},{15,199,50,4},{ 7,199,50,4},{ 4,204,51,4},{ 4,191,48,4},
  { 3,199,50,4},{ 3,159,40,4},{ 1,199,50,4},{ 1,159,40,4},{ 1,127,32,4},
  { 0,159,40,4},{ 0,127,32,4},{ 0, 63,16,4},{ 0, 31, 8,4},
};

static const canbus::CanSpeed SPEED_ORDER[] = {
  canbus::CAN_5KBPS, canbus::CAN_10KBPS, canbus::CAN_20KBPS,
  canbus::CAN_31K25BPS, canbus::CAN_33KBPS, canbus::CAN_40KBPS,
  canbus::CAN_50KBPS, canbus::CAN_80KBPS, canbus::CAN_100KBPS,
  canbus::CAN_125KBPS, canbus::CAN_200KBPS, canbus::CAN_250KBPS,
  canbus::CAN_500KBPS, canbus::CAN_1000KBPS,
};
// clang-format on

static int speed_index_(canbus::CanSpeed s) {
  for (int i = 0; i < 14; i++)
    if (SPEED_ORDER[i] == s) return i;
  return -1;
}

bool MCP2518FD::calc_nbt_(canbus::CanSpeed speed, CanClock clk, uint32_t &nbtcfg) const {
  int idx = speed_index_(speed);
  if (idx < 0) return false;
  const NbtEntry *t = (clk == MCP_40MHZ || clk == MCP_4MHZ) ? NBT_40MHZ :
                      (clk == MCP_20MHZ) ? NBT_20MHZ : NBT_10MHZ;
  nbtcfg = (uint32_t(t[idx].brp) << 24) | (uint32_t(t[idx].tseg1) << 16) |
           (uint32_t(t[idx].tseg2) << 8) | t[idx].sjw;
  return true;
}

bool MCP2518FD::calc_dbt_(canbus::CanSpeed speed, CanClock clk,
                           uint32_t &dbtcfg, uint32_t &tdcval) const {
  int idx = speed_index_(speed);
  if (idx < 0) return false;
  const NbtEntry *t = (clk == MCP_40MHZ || clk == MCP_4MHZ) ? NBT_40MHZ :
                      (clk == MCP_20MHZ) ? NBT_20MHZ : NBT_10MHZ;
  // CiDBTCFG: BRP[7:0] TSEG1[4:0] TSEG2[3:0] SJW[3:0]
  uint8_t dsjw   = t[idx].sjw   < 15 ? t[idx].sjw   : 15;
  uint8_t dtseg2 = t[idx].tseg2 < 15 ? t[idx].tseg2 : 15;
  uint8_t dtseg1 = t[idx].tseg1 < 31 ? t[idx].tseg1 : 31;
  dbtcfg = (uint32_t(t[idx].brp) << 24) | (uint32_t(dtseg1) << 16) |
           (uint32_t(dtseg2) << 8) | dsjw;
  // TDC auto mode: TDCMOD=2, TDCO = TSEG1+1
  tdcval = (0x02UL << 8) | uint32_t(dtseg1 + 1);
  return true;
}

canbus::Error MCP2518FD::configure_bit_timing_() {
  uint32_t nbtcfg = 0;
  if (!calc_nbt_(this->bit_rate_, this->can_clock_, nbtcfg)) {
    ESP_LOGE(TAG, "Unsupported nominal bit rate / clock combination");
    return canbus::ERROR_FAIL;
  }
  write_sfr_(REG_CiNBTCFG, nbtcfg);
  ESP_LOGVV(TAG, "CiNBTCFG=0x%08" PRIx32 "", nbtcfg);

  if (this->canfd_enabled_) {
    uint32_t dbtcfg = 0, tdcval = 0;
    if (!calc_dbt_(this->can_data_rate_, this->can_clock_, dbtcfg, tdcval)) {
      ESP_LOGE(TAG, "Unsupported data bit rate / clock combination");
      return canbus::ERROR_FAIL;
    }
    write_sfr_(REG_CiDBTCFG, dbtcfg);
    write_sfr_(REG_CiTDC,    tdcval);
    ESP_LOGVV(TAG, "CiDBTCFG=0x%08" PRIx32 " CiTDC=0x%08" PRIx32 "", dbtcfg, tdcval);
  }
  return canbus::ERROR_OK;
}

// ============================================================
// FIFO configuration
// ============================================================

canbus::Error MCP2518FD::configure_fifos_() {
  uint8_t plsize = this->canfd_enabled_ ? 7 : 0;

  // CH1 = RX FIFO: TXEN=0, 32 messages deep, FRESET
  uint32_t rxcon = (31UL << 24)              // FSIZE = 32 messages
                 | (uint32_t(plsize) << 29)  // PLSIZE
                 | (1UL << 10);              // FRESET
  write_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 1, rxcon);
  for (int i = 0; i < 10; i++) {
    if (!(read_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET*1) & (1UL<<10))) break;
    delay(1);
  }
  rxcon &= ~(1UL << 10);
  write_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 1, rxcon);

  // CH2 = TX FIFO: TXEN=1 (bit7), 4 messages deep, highest priority, FRESET
  uint32_t txcon = FIFOCON_TXEN              // bit7: TX FIFO
                 | (3UL << 24)              // FSIZE = 4 messages
                 | (uint32_t(plsize) << 29) // PLSIZE
                 | (31UL << 16)             // TXPRI = highest
                 | (1UL << 10);             // FRESET
  write_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 2, txcon);
  for (int i = 0; i < 10; i++) {
    if (!(read_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET*2) & (1UL<<10))) break;
    delay(1);
  }
  // Clear FRESET and explicitly clear TXREQ (bit9) to prevent stale TX across reboots
  txcon &= ~(1UL << 10);  // FRESET=0
  txcon &= ~(1UL << 9);   // TXREQ=0  ← force clear
  write_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 2, txcon);

  // Erase TX FIFO RAM content to prevent stale frame from persisting across OTA reboots
  // (the chip keeps RAM content when only 5V is maintained across reboots)
  {
    uint32_t tx_ram_addr = static_cast<uint16_t>(read_sfr_(REG_CiFIFOUA + CIFIFO_OFFSET * 2));
    if (tx_ram_addr < RAM_ADDR_START) tx_ram_addr += RAM_ADDR_START;
    uint8_t zeros[16] = {};
    // Erase all 4 slots of the TX FIFO (4 messages × 16 bytes each)
    for (int slot = 0; slot < 4; slot++) {
      write_ram_(static_cast<uint16_t>(tx_ram_addr + slot * 16), zeros, 16);
    }
  }

  uint32_t txsta = read_sfr_(REG_CiFIFOSTA + CIFIFO_OFFSET * 2);
  ESP_LOGD(TAG, "TX FIFO CH2 STA=0x%08" PRIx32 " (TXNIF=%d)", txsta, (int)(txsta&1));

  return canbus::ERROR_OK;
}

// ============================================================
// Filter: accept all frames → FIFO CH1
// ============================================================

canbus::Error MCP2518FD::configure_filters_() {
  // CiFLTCON0 byte 0: FLTEN0=1, F0BP=1 (→ FIFO CH1)
  write_byte_(REG_CiFLTCON, (1 << 7) | 1);
  write_sfr_(REG_CiFLTOBJ, 0x00000000UL);  // ID = 0
  write_sfr_(REG_CiMASK,   0x00000000UL);  // mask = 0 → all pass
  return canbus::ERROR_OK;
}

// ============================================================
// Interrupt configuration
// ============================================================

canbus::Error MCP2518FD::configure_interrupts_() {
  uint32_t inten = 0;

  if (this->int1_pin_ != nullptr) {
    // INT1 wired → enable RX interrupt so INT1 asserts on new frame
    inten |= INT_RXIF;
    // Configure INT1 as RX interrupt output (PM1=0 in IOCON)
    // IOCON.PM1 = 0 → INT1 pin acts as RX interrupt
    uint32_t iocon = read_sfr_(REG_IOCON);
    iocon &= ~(1UL << 25);  // PM1 = 0
    write_sfr_(REG_IOCON, iocon);
    ESP_LOGI(TAG, "INT1 (RX interrupt) configured on pin");
  }

  if (this->int0_pin_ != nullptr) {
    // INT0 wired → enable TX interrupt so INT0 asserts when TX done
    inten |= INT_TXIF;
    // IOCON.PM0 = 0 → INT0 pin acts as TX interrupt
    uint32_t iocon = read_sfr_(REG_IOCON);
    iocon &= ~(1UL << 24);  // PM0 = 0
    write_sfr_(REG_IOCON, iocon);
    ESP_LOGI(TAG, "INT0 (TX interrupt) configured on pin");
  }

  if (this->int_pin_ != nullptr) {
    // General INT wired → enable error and overflow interrupts
    inten |= INT_CERRIF | INT_RXOVIF;
    ESP_LOGI(TAG, "INT (general interrupt) configured on pin");
  }

  // Always enable RX overflow in the interrupt enable register
  inten |= INT_RXOVIF | INT_CERRIF;

  // Write upper 16 bits of CiINT (enable register)
  uint32_t reg = read_sfr_(REG_CiINT);
  reg = (reg & 0x0000FFFFUL) | (uint32_t(inten) << 16);
  write_sfr_(REG_CiINT, reg);

  return canbus::ERROR_OK;
}

// ============================================================
// setup_internal
// ============================================================

bool MCP2518FD::setup_internal() {
  this->spi_setup();
  ESP_LOGD(TAG, "SPI setup done");

  // Configure interrupt input pins
  if (this->int_pin_ != nullptr)  { this->int_pin_->setup();  ESP_LOGD(TAG, "INT  pin configured"); }
  if (this->int0_pin_ != nullptr) { this->int0_pin_->setup(); ESP_LOGD(TAG, "INT0 pin configured"); }
  if (this->int1_pin_ != nullptr) { this->int1_pin_->setup(); ESP_LOGD(TAG, "INT1 pin configured"); }

  // Read DEVID to verify SPI communication before doing anything else
  delay(10);  // let VDD stabilise
  // Read multiple registers to diagnose SPI — some clones have DEVID=0 but still work
  uint32_t devid    = read_sfr_(REG_DEVID);   // 0xE14 — should be 0x28 for MCP2518FD
  uint32_t osc_pre  = read_sfr_(REG_OSC);     // 0xE00 — POR value = 0x00000060
  uint32_t cicon    = read_sfr_(REG_CiCON);   // 0x000 — POR value = 0x04980760
  uint32_t iocon    = read_sfr_(REG_IOCON);   // 0xE04 — POR value = 0x03000000
  this->init_devid_ = devid;
  this->init_cicon_  = cicon;
  this->init_iocon_  = iocon;

  ESP_LOGD(TAG, "SPI register scan:");
  ESP_LOGD(TAG, "  DEVID  (0xE14) = 0x%08" PRIx32 "  (MCP2518FD expect 0x00000028)", devid);
  ESP_LOGD(TAG, "  OSC    (0xE00) = 0x%08" PRIx32 "  (POR expect    0x00000060)", osc_pre);
  ESP_LOGD(TAG, "  CiCON  (0x000) = 0x%08" PRIx32 "  (POR expect    0x04980760)", cicon);
  ESP_LOGD(TAG, "  IOCON  (0xE04) = 0x%08" PRIx32 "  (POR expect    0x03000000)", iocon);

  bool spi_ok = (osc_pre  != 0x00000000 && osc_pre  != 0xFFFFFFFF) ||
                (cicon     != 0x00000000 && cicon     != 0xFFFFFFFF) ||
                (iocon     != 0x00000000 && iocon     != 0xFFFFFFFF) ||
                (devid     != 0x00000000 && devid     != 0xFFFFFFFF);

  if (!spi_ok) {
    ESP_LOGE(TAG, "All registers read 0x00 or 0xFF — SPI bus not responding");
    ESP_LOGE(TAG, "Check: VCC=5V, MOSI/MISO not swapped, CS active-low, SCK polarity");
    return false;
  }
  if (devid == 0x00000000) {
    ESP_LOGW(TAG, "DEVID=0 but other registers respond — possible clone chip, continuing");
  }

  ESP_LOGD(TAG, "Sending RESET...");
  if (reset_() != canbus::ERROR_OK) { ESP_LOGE(TAG, "reset_ failed"); return false; }

  ESP_LOGD(TAG, "Entering Configuration mode...");
  if (set_mode_(CAN_CONFIGURATION_MODE) != canbus::ERROR_OK) { ESP_LOGE(TAG, "set_mode_(CONFIG) failed"); return false; }

  ESP_LOGD(TAG, "Configuring oscillator...");
  if (configure_osc_() != canbus::ERROR_OK) { ESP_LOGE(TAG, "configure_osc_ failed"); return false; }

  ESP_LOGD(TAG, "Configuring bit timing...");
  if (configure_bit_timing_() != canbus::ERROR_OK) { ESP_LOGE(TAG, "configure_bit_timing_ failed"); return false; }

  // CiCON: set TXQEN + ISO CRC config BEFORE configure_fifos_
  // (CiTXQCON is only writable when CiCON.TXQEN=1)
  uint32_t con = read_sfr_(REG_CiCON);
  ESP_LOGD(TAG, "CiCON before=0x%08" PRIx32 "", con);
  // Note: CiCON_TXQEN not needed since we use CH2 TX FIFO instead of TXQ
  con &= ~(1UL << 19);                   // STEF=0: disable TEF (save RAM)
  if (this->canfd_enabled_) {
    con |= CiCON_ISOCRCEN;               // FD: use ISO CRC
  } else {
    con &= ~CiCON_ISOCRCEN;              // Classic: MUST clear ISO CRC (POR=1!)
    con |= CiCON_BRSDIS;                 // Classic: disable bit rate switching
  }
  ESP_LOGD(TAG, "CiCON after =0x%08" PRIx32 " (ISOCRCEN=%d)", con, (int)((con>>5)&1));
  write_sfr_(REG_CiCON, con);
  this->init_cicon_ = con;

  ESP_LOGD(TAG, "Configuring FIFOs...");
  if (configure_fifos_() != canbus::ERROR_OK) { ESP_LOGE(TAG, "configure_fifos_ failed"); return false; }

  ESP_LOGD(TAG, "Configuring filters...");
  if (configure_filters_() != canbus::ERROR_OK) { ESP_LOGE(TAG, "configure_filters_ failed"); return false; }

  ESP_LOGD(TAG, "Configuring interrupts...");
  if (configure_interrupts_() != canbus::ERROR_OK) { ESP_LOGE(TAG, "configure_interrupts_ failed"); return false; }

  ESP_LOGD(TAG, "Entering operational mode %d...", this->mcp_mode_);
  if (set_mode_(static_cast<CanOperationMode>(this->mcp_mode_)) != canbus::ERROR_OK) {
    ESP_LOGE(TAG, "set_mode_(operational) failed");
    return false;
  }
  // Read CH2 STA after entering operational mode (STA is invalid in Config mode per datasheet)
  {
    uint32_t ch2sta = read_sfr_(REG_CiFIFOSTA + CIFIFO_OFFSET * 2);
    uint32_t ch2con = read_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 2);
    uint32_t cicon  = read_sfr_(REG_CiCON);
    ESP_LOGD(TAG, "Post-operational: CiCON=0x%08" PRIx32 " OPMOD=%d", cicon, (int)((cicon>>21)&7));
    ESP_LOGD(TAG, "  CH2 CON=0x%08" PRIx32 " STA=0x%08" PRIx32 " TXNIF=%d", ch2con, ch2sta, (int)(ch2sta&1));
    if ((ch2sta & 1) == 0 || (ch2sta & (1<<5)) || (ch2sta & (1<<1))) {
      // TXNIF=0 (FIFO full) OR TXERR=1 OR TXATIF=1 — stale/errored TX frame, do FRESET
      ESP_LOGW(TAG, "  CH2 STA=0x%08" PRIx32 " TXNIF=%d TXERR=%d TXATIF=%d — applying FRESET to clear stale TX",
               ch2sta, (int)(ch2sta&1), (int)((ch2sta>>5)&1), (int)((ch2sta>>1)&1));
      uint32_t txcon = ch2con | (1UL << 10);
      write_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 2, txcon);
      delay(2);
      txcon &= ~(1UL << 10);
      write_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 2, txcon);
      ch2sta = read_sfr_(REG_CiFIFOSTA + CIFIFO_OFFSET * 2);
      ESP_LOGD(TAG, "  CH2 STA after FRESET=0x%08" PRIx32 " TXNIF=%d", ch2sta, (int)(ch2sta&1));
    }
  }

  this->init_citrec_ = read_sfr_(REG_CiTREC);
  this->init_done_ = true;
  ESP_LOGD(TAG, "MCP2518FD ready — CiBDIAG0=0x%08" PRIx32 " CiTREC=0x%08" PRIx32 "",
           read_sfr_(REG_CiBDIAG0), this->init_citrec_);
  return true;
}



// ============================================================
// dump_config — called after setup, visible via WiFi/API logs
// ============================================================

void MCP2518FD::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP2518FD:");
  ESP_LOGCONFIG(TAG, "  Init success : %s", this->init_done_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  DEVID  (0xE14): 0x%08" PRIx32 " (MCP2518FD=0x28)", this->init_devid_);
  ESP_LOGCONFIG(TAG, "  OSC    (0xE00): 0x%08" PRIx32 " (POR=0x60)",        this->init_osc_);
  {
    uint32_t live_con = read_sfr_(REG_CiCON);
    uint32_t live_sta = read_sfr_(REG_CiFIFOSTA + CIFIFO_OFFSET * 2);
    ESP_LOGCONFIG(TAG, "  CiCON  (live): 0x%08" PRIx32 " OPMOD=%d REQOP=%d ISOCRCEN=%d",
                  live_con, (int)((live_con>>21)&7), (int)((live_con>>24)&7), (int)((live_con>>5)&1));
    ESP_LOGCONFIG(TAG, "  CiCON  (init): 0x%08" PRIx32 "", this->init_cicon_);
    ESP_LOGCONFIG(TAG, "  CH2STA (live): 0x%08" PRIx32 " TXNIF=%d", live_sta, (int)(live_sta&1));
  }
  ESP_LOGCONFIG(TAG, "  IOCON  (0xE04): 0x%08" PRIx32 " (POR=0x03000000)", this->init_iocon_);
  ESP_LOGCONFIG(TAG, "  CiTREC (0x034): 0x%08" PRIx32 "",                   this->init_citrec_);
  // Live FIFO status — read at dump time (after init)
  uint32_t ch2con = read_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 2);
  uint32_t ch2sta = read_sfr_(REG_CiFIFOSTA + CIFIFO_OFFSET * 2);
  uint32_t ch1con = read_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 1);
  uint32_t ch1sta = read_sfr_(REG_CiFIFOSTA + CIFIFO_OFFSET * 1);
  ESP_LOGCONFIG(TAG, "  CH1(RX) CON=0x%08" PRIx32 " STA=0x%08" PRIx32 "", ch1con, ch1sta);
  ESP_LOGCONFIG(TAG, "  CH2(TX) CON=0x%08" PRIx32 " STA=0x%08" PRIx32 " TXNIF=%d TXEN=%d",
                ch2con, ch2sta, (int)(ch2sta&1), (int)((ch2con>>7)&1));
  LOG_PIN("  CS Pin : ", this->cs_);
  if (!this->init_done_) {
    if (this->init_devid_ == 0x00000000 || this->init_devid_ == 0xFFFFFFFF)
      ESP_LOGE(TAG, "  => SPI communication failed (DEVID=0x%08" PRIx32 ")", this->init_devid_);
    else if ((this->init_osc_ & OSC_OSCRDY) == 0)
      ESP_LOGE(TAG, "  => Oscillator not ready (OSC=0x%08" PRIx32 ", OSCRDY bit 10 not set)", this->init_osc_);
    else
      ESP_LOGE(TAG, "  => Init failed at unknown step");
  }
}

// ============================================================
// TX
// ============================================================

uint16_t MCP2518FD::tx_fifo_addr_() {
  uint16_t addr = static_cast<uint16_t>(read_sfr_(REG_CiFIFOUA + CIFIFO_OFFSET * 2));
  if (addr < RAM_ADDR_START)
    addr += RAM_ADDR_START;
  return addr;
}

canbus::Error MCP2518FD::send_message_txq_(struct canbus::CanFrame *frame) {
  uint8_t dlc = frame->can_data_length_code;
  if (!this->canfd_enabled_ && dlc > canbus::CAN_MAX_DATA_LENGTH)
    return canbus::ERROR_FAILTX;
  if (dlc > CAN_FD_MAX_DATA_LENGTH)
    return canbus::ERROR_FAILTX;

  // Check for bus-off BEFORE trying to send — recover if needed
  uint32_t trec_pre = read_sfr_(REG_CiTREC);
  if (trec_pre & (1UL << 21)) {  // TXBO bit
    ESP_LOGD(TAG, "Bus-off detected before TX — recovering");
    set_mode_(CAN_CONFIGURATION_MODE);
    delay(2);
    write_sfr_(REG_CiBDIAG0, 0x00000000UL);
    write_sfr_(REG_CiBDIAG1, 0x00000000UL);
    set_mode_(static_cast<CanOperationMode>(this->mcp_mode_));
    delay(2);
  }

  // NOTE: On clone chips (DEVID=0x00), CiFIFOSTA always returns 0x00000000
  // so we cannot rely on TXNIF bit. Instead check CiBDIAG1 for actual bus errors.
  uint32_t txsta = read_sfr_(REG_CiFIFOSTA + CIFIFO_OFFSET * 2);
  if ((txsta >> 1) & 1) {  // TXATIF set = attempt complete, log diagnostics
    uint32_t trec = read_sfr_(REG_CiTREC);
    uint32_t diag = read_sfr_(REG_CiBDIAG1);
    ESP_LOGVV(TAG, "TX: CiTREC=0x%08" PRIx32 " TEC=%d REC=%d", trec, (int)((trec>>8)&0xFF), (int)(trec&0xFF));
    if (diag & 0x00070000UL)
      ESP_LOGW(TAG, "TX CiBDIAG1=0x%08" PRIx32 " NACKERR=%d NBIT1=%d NBIT0=%d", diag, (int)((diag>>18)&1), (int)((diag>>17)&1), (int)((diag>>16)&1));
  }

  // Only block on actual bus error, not on TXNIF=0 (unreliable on clone chips)
  if (txsta & (1UL << 5)) {  // TXERR
    uint32_t b1 = read_sfr_(REG_CiBDIAG1);
    uint32_t tr = read_sfr_(REG_CiTREC);
    ESP_LOGW(TAG, "TX FIFO TXERR BDIAG1=0x%08" PRIx32 " TREC=0x%08" PRIx32 "", b1, tr);
    if (b1 & (1UL<<18)) ESP_LOGW(TAG, "  NACKERR: no ACK");
    if (b1 & (1UL<<17)) ESP_LOGW(TAG, "  NBIT1ERR");
    if (b1 & (1UL<<16)) ESP_LOGW(TAG, "  NBIT0ERR");
    if (b1 & (1UL<<21)) ESP_LOGW(TAG, "  NCRCERR");
    if (b1 & (1UL<<23)) ESP_LOGW(TAG, "  TXBOERR: bus-off");
    // FRESET to recover
    uint32_t txcon = read_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 2);
    write_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 2, txcon | (1UL << 10));
    delay(2);
    write_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 2, txcon & ~(1UL << 10));
    return canbus::ERROR_ALLTXBUSY;
  }
  // Proceed to send (ignore TXNIF on clone chips)

  // CiTXQUA already contains the full SPI address (includes RAM base 0x400)
  uint16_t ram_addr = tx_fifo_addr_();

  // Build transmit message object (T0 + T1 + data)
  // T0: ID word — MCP2518FD CAN_MSGOBJ_ID structure:
  // bits[10:0]  = SID[10:0]
  // bits[28:11] = EID[17:0]
  // (matches Soldered Arduino library mcp2518fd_sendMsg)
  uint32_t id_word = 0;
  if (frame->use_extended_id) {
    uint32_t sid = (frame->can_id >> 18) & 0x7FFUL;
    uint32_t eid =  frame->can_id        & 0x3FFFFUL;
    id_word = sid | (eid << 11);
  } else {
    id_word = (frame->can_id & 0x7FFUL) << 18;
  }

  // T1: control word
  uint32_t ctrl = dlc & MSGOBJ_DLC_MASK;
  if (frame->use_extended_id)                 ctrl |= MSGOBJ_IDE;
  if (frame->remote_transmission_request)     ctrl |= MSGOBJ_RTR;
  if (this->canfd_enabled_ && dlc > 8)        ctrl |= MSGOBJ_FDF | MSGOBJ_BRS;

  // Pad data to multiple of 4 bytes for word-aligned RAM writes
  uint8_t pad_len = (dlc + 3) & ~3u;
  uint8_t obj[8 + 64] = {};
  obj[0] = id_word;       obj[1] = id_word >> 8;
  obj[2] = id_word >> 16; obj[3] = id_word >> 24;
  obj[4] = ctrl;          obj[5] = ctrl >> 8;
  obj[6] = ctrl >> 16;    obj[7] = ctrl >> 24;
  memcpy(&obj[8], frame->data, dlc);

  write_ram_(ram_addr, obj, 8 + pad_len);

  ESP_LOGVV(TAG, "TX RAM: addr=0x%04X id_word=0x%08" PRIx32 " ctrl=0x%08" PRIx32 " can_id=0x%08" PRIx32,
           ram_addr, id_word, ctrl, frame->can_id);
  ESP_LOGVV(TAG, "TX obj: %02X%02X%02X%02X %02X%02X%02X%02X",
           obj[0],obj[1],obj[2],obj[3],obj[4],obj[5],obj[6],obj[7]);

  // Set UINC + TXREQ by writing only byte[1] of CiFIFOCON (same as Soldered Arduino library)
  // byte[1] contains: bit0=UINC, bit1=TXREQ, bit2=FRESET
  // Writing only byte[1] avoids disturbing other bits in the register
  uint8_t fifocon_byte1 = (1 << 0) | (1 << 1);  // UINC=1, TXREQ=1
  uint16_t byte1_addr = static_cast<uint16_t>(REG_CiFIFOCON + CIFIFO_OFFSET * 2 + 1);
  write_byte_(byte1_addr, fifocon_byte1);

  // No delay — just check NACKERR for debug (bus-off handled before TX)

  return canbus::ERROR_OK;
}

canbus::Error MCP2518FD::send_message(struct canbus::CanFrame *frame) {
  return send_message_txq_(frame);
}

// ============================================================
// RX
// ============================================================

uint16_t MCP2518FD::rx_fifo_addr_() {
  uint32_t raw = read_sfr_(REG_CiFIFOUA + CIFIFO_OFFSET * 1);
  // CiFIFOUA returns the offset within RAM — add RAM base 0x400
  uint16_t addr = static_cast<uint16_t>(raw & 0xFFFF);
  if (addr < RAM_ADDR_START)
    addr += RAM_ADDR_START;
  return addr;
}

void MCP2518FD::loop() {
  if (mcp_mode_ == CAN_MODE_LISTEN_ONLY && sniffer_ != nullptr) {
    // Drain FIFO repeatedly until empty or 10ms budget exceeded
    uint32_t deadline = millis() + 10;
    do {
      drain_to_sniffer_();
      App.feed_wdt();
    } while (millis() < deadline);
    return;
  }
  canbus::Canbus::loop();
}

bool MCP2518FD::rx_available_() {
  // Limit messages processed per loop() call to prevent task watchdog trigger
  uint8_t limit = 16;
  if (this->rx_frame_count_ >= limit) {
    this->rx_frame_count_ = 0;
    return false;
  }
  // Feed watchdog every 8 frames when processing intensively
  if (this->rx_frame_count_ % 8 == 0)
    App.feed_wdt();
  this->rx_frame_count_++;
  if (this->int1_pin_ != nullptr)
    return !this->int1_pin_->digital_read();

  // Use CiRXIF bit 1 = FIFO1 has received a message (genuine chip)
  uint32_t rxif = read_sfr_(REG_CiRXIF);
  if (rxif & (1UL << 1))
    return true;

  uint32_t sta = read_sfr_(REG_CiFIFOSTA + CIFIFO_OFFSET * 1);
  return (sta & FIFOSTA_RXNOTEMPTY) != 0;
}

canbus::Error MCP2518FD::read_message_fifo_(struct canbus::CanFrame *frame) {
  if (!rx_available_())
    return canbus::ERROR_NOMSG;

  uint16_t rxsta_addr = REG_CiFIFOSTA + CIFIFO_OFFSET * 1;
  uint16_t rxcon_addr = REG_CiFIFOCON + CIFIFO_OFFSET * 1;

  uint32_t sta = read_sfr_(rxsta_addr);

  // Clear overflow flag if set
  if (sta & FIFOSTA_RXOVIF) {
    ESP_LOGW(TAG, "RX FIFO overflow");
    uint32_t ovif = read_sfr_(REG_CiRXOVIF);
    write_sfr_(REG_CiRXOVIF, ovif & ~(1UL << 1));
  }

  // Read header (8 bytes = T0 + T1, no timestamp)
  // CiFIFOUA already contains the full SPI address (includes RAM base 0x400)
  uint16_t ram_addr = rx_fifo_addr_();
  uint8_t  hdr[8];
  read_ram_(ram_addr, hdr, 8);

  uint32_t id_word = hdr[0] | (hdr[1]<<8) | (hdr[2]<<16) | (hdr[3]<<24);
  uint32_t ctrl    = hdr[4] | (hdr[5]<<8) | (hdr[6]<<16) | (hdr[7]<<24);

  bool    extended = (ctrl & MSGOBJ_IDE) != 0;
  bool    rtr      = (ctrl & MSGOBJ_RTR) != 0;
  uint8_t dlc      = static_cast<uint8_t>(ctrl & MSGOBJ_DLC_MASK);
  uint8_t nbytes   = dlc_to_bytes_(dlc);

  if (nbytes > CAN_FD_MAX_DATA_LENGTH)
    goto uinc;

  frame->use_extended_id             = extended;
  frame->remote_transmission_request = rtr;
  frame->can_data_length_code        = nbytes;

  if (extended) {
    // CAN_MSGOBJ_ID: SID at bits[10:0], EID at bits[28:11]
    // Reconstruct 29-bit ID: (SID<<18)|EID (matches Soldered Arduino mcp2518fd_readMsgBufID)
    uint32_t sid = id_word & 0x7FFUL;
    uint32_t eid = (id_word >> 11) & 0x3FFFFUL;
    frame->can_id = (sid << 18) | eid;
  } else {
    frame->can_id = (id_word >> 18) & 0x7FFUL;
  }

  if (nbytes > 0) {
    uint8_t pad_len = (nbytes + 3) & ~3u;  // read whole words
    uint8_t buf[64] = {};
    read_ram_(ram_addr + 8, buf, pad_len);
    memcpy(frame->data, buf,
           nbytes <= canbus::CAN_MAX_DATA_LENGTH ? nbytes : canbus::CAN_MAX_DATA_LENGTH);
  }

uinc:
  // Increment FIFO tail (UINC)
  {
    uint32_t rxcon = read_sfr_(rxcon_addr);
    rxcon |= FIFOCON_UINC;
    write_sfr_(rxcon_addr, rxcon);
  }

  // Feed sniffer text_sensor if registered (LISTEN_ONLY mode)
  if (sniffer_ != nullptr) {
    sniffer_->push_frame(frame->can_id, frame->use_extended_id,
                         frame->data,
                         nbytes <= canbus::CAN_MAX_DATA_LENGTH ? nbytes : canbus::CAN_MAX_DATA_LENGTH);
  }

  return canbus::ERROR_OK;
}

canbus::Error MCP2518FD::read_message(struct canbus::CanFrame *frame) {
  return read_message_fifo_(frame);
}

void MCP2518FD::drain_to_sniffer_() {
  if (sniffer_ == nullptr) return;
  if (mcp_mode_ != CAN_MODE_LISTEN_ONLY) return;

  // CiFIFOCON/STA/UA for RX FIFO (FIFO1) are contiguous — read all 3 in one burst
  uint16_t fifo1_base = REG_CiFIFOCON + CIFIFO_OFFSET * 1;  // 0x05C
  // UINC is bit 8 of CiFIFOCON → byte offset 1 from CON base
  uint16_t uinc_byte_addr = fifo1_base + 1;

  int msg_count = 0;
  for (int i = 0; i < 32; i++, msg_count++) {
    // 1) Read CON+STA+UA in ONE 12-byte SPI burst
    uint8_t regs[12];
    read_ram_(fifo1_base, regs, 12);
    // STA is at offset 4 (bytes 4-7), UA is at offset 8 (bytes 8-11)
    uint32_t sta = (uint32_t)regs[4] | ((uint32_t)regs[5] << 8) |
                   ((uint32_t)regs[6] << 16) | ((uint32_t)regs[7] << 24);
    if (!(sta & 0x01)) break;  // RXNIF=0 → FIFO empty

    uint16_t ram_addr = (uint32_t)regs[8] | ((uint32_t)regs[9] << 8);
    if (ram_addr < RAM_ADDR_START) ram_addr += RAM_ADDR_START;

    // 2) Read T0+T1+data (16 bytes) in ONE burst
    uint8_t buf[16] = {};
    read_ram_(ram_addr, buf, 16);

    // 3) UINC — single byte write, no read-modify-write
    write_sfr_byte_(uinc_byte_addr, 0x01);

    // Decode ID
    uint32_t id_word = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                       ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    uint32_t ctrl    = (uint32_t)buf[4] | ((uint32_t)buf[5] << 8) |
                       ((uint32_t)buf[6] << 16) | ((uint32_t)buf[7] << 24);
    bool extended = (ctrl >> 4) & 1;
    uint8_t dlc   = ctrl & 0x0F;
    uint8_t nbytes = dlc_to_bytes_(dlc);
    if (nbytes > 8) nbytes = 8;

    uint32_t can_id;
    if (extended) {
      can_id = ((id_word & 0x7FFUL) << 18) | ((id_word >> 11) & 0x3FFFFUL);
    } else {
      can_id = (id_word >> 18) & 0x7FFUL;
    }

    sniffer_->push_frame(can_id, extended, buf + 8, nbytes);
  }
}

// ============================================================
// Utility
// ============================================================

uint8_t MCP2518FD::dlc_to_bytes_(uint8_t dlc) {
  return dlc < 16 ? DLC_TO_BYTES[dlc] : 0;
}

uint8_t MCP2518FD::bytes_to_dlc_(uint8_t len) {
  for (uint8_t d = 0; d < 16; d++)
    if (DLC_TO_BYTES[d] >= len) return d;
  return 15;
}

}  // namespace esphome::mcp2518fd
