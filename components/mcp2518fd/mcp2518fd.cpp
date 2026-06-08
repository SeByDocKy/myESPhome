#include "mcp2518fd.h"
#include "esphome/core/log.h"

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
  uint8_t cmd[2];
  build_cmd_(addr, MCP2518FD_INSTRUCTION_READ, cmd);
  this->enable();
  this->transfer_byte(cmd[0]);
  this->transfer_byte(cmd[1]);
  for (uint8_t i = 0; i < n; i++)
    data[i] = this->transfer_byte(0x00);
  this->disable();
}

void MCP2518FD::write_ram_(uint16_t addr, const uint8_t *data, uint8_t n) {
  uint8_t cmd[2];
  build_cmd_(addr, MCP2518FD_INSTRUCTION_WRITE, cmd);
  this->enable();
  this->transfer_byte(cmd[0]);
  this->transfer_byte(cmd[1]);
  for (uint8_t i = 0; i < n; i++)
    this->transfer_byte(data[i]);
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

  // Step 3: Send SPI RESET instruction (0x00 0x00)
  this->enable();
  this->transfer_byte(0x00);
  this->transfer_byte(0x00);
  this->disable();

  // Step 4: Wait for oscillator to stabilize (Linux uses poll, we use fixed delay)
  delay(10);

  // Step 5: Verify OSC is ready — POR default is 0x00000060 (CLKODIV=10, OSCRDY=1)
  uint32_t osc = read_sfr_(REG_OSC);
  ESP_LOGD(TAG, "reset_() done — OSC=0x%08X (expect ~0x00000460 after POR)", osc);

  return canbus::ERROR_OK;
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
      ESP_LOGD(TAG, "OSC ready — OSC=0x%08X", status);
      this->init_osc_ = status;
      return canbus::ERROR_OK;
    }
  }
  uint32_t final_osc = read_sfr_(REG_OSC);
  ESP_LOGE(TAG, "OSC/PLL not ready — OSC=0x%08X (OSCRDY bit10=%d PLLRDY bit8=%d)",
           final_osc,
           (final_osc >> 10) & 1,
           (final_osc >>  8) & 1);
  return canbus::ERROR_FAIL;
}

// ============================================================
// Bit timing
// ============================================================

struct NbtEntry { uint8_t brp, tseg1, tseg2, sjw; };

// clang-format off
static const NbtEntry NBT_40MHZ[] = {
  {99,31,8,8},{49,31,8,8},{24,31,8,8},{15,31,8,8},{14,31,8,8},
  {12,31,6,6},{9,31,8,8},{6,31,8,8},{4,31,8,8},
  {1,126,33,33},{1,74,25,25},{0,126,33,33},{0,62,17,17},{0,30,9,9},
};
static const NbtEntry NBT_20MHZ[] = {
  {49,31,8,8},{24,31,8,8},{12,31,6,6},{7,31,8,8},{7,29,8,8},
  {6,31,6,6},{4,31,8,8},{3,31,6,6},{2,31,6,6},
  {0,126,33,33},{0,74,25,25},{0,62,17,17},{0,30,9,9},{0,14,5,5},
};
static const NbtEntry NBT_10MHZ[] = {
  {24,31,8,8},{12,31,6,6},{6,31,6,6},{3,31,8,8},{3,29,8,8},
  {3,23,7,7},{2,31,6,6},{1,31,6,6},{1,23,6,6},
  {0,62,17,17},{0,36,13,13},{0,30,9,9},{0,14,5,5},{0,6,3,3},
};
static const NbtEntry NBT_4MHZ[] = {   // same Fosc=40 MHz via PLL
  {99,31,8,8},{49,31,8,8},{24,31,8,8},{15,31,8,8},{14,31,8,8},
  {12,31,6,6},{9,31,8,8},{6,31,8,8},{4,31,8,8},
  {1,126,33,33},{1,74,25,25},{0,126,33,33},{0,62,17,17},{0,30,9,9},
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
  ESP_LOGV(TAG, "CiNBTCFG=0x%08X", nbtcfg);

  if (this->canfd_enabled_) {
    uint32_t dbtcfg = 0, tdcval = 0;
    if (!calc_dbt_(this->can_data_rate_, this->can_clock_, dbtcfg, tdcval)) {
      ESP_LOGE(TAG, "Unsupported data bit rate / clock combination");
      return canbus::ERROR_FAIL;
    }
    write_sfr_(REG_CiDBTCFG, dbtcfg);
    write_sfr_(REG_CiTDC,    tdcval);
    ESP_LOGV(TAG, "CiDBTCFG=0x%08X CiTDC=0x%08X", dbtcfg, tdcval);
  }
  return canbus::ERROR_OK;
}

// ============================================================
// FIFO configuration
// ============================================================

canbus::Error MCP2518FD::configure_fifos_() {
  // TXQ (FIFO CH0): 4 messages deep, payload 8 or 64 bytes
  uint8_t  plsize = this->canfd_enabled_ ? 7 : 0;
  uint32_t txqcon = FIFOCON_TXEN
                  | (3UL  <<  24)  // FSIZE = 4 messages
                  | (uint32_t(plsize) << 29)  // PLSIZE
                  | (31UL << 16);  // TXPRI = highest
  write_sfr_(REG_CiTXQCON, txqcon);

  // RX FIFO CH1: 8 messages deep, same payload
  uint32_t rxcon = (7UL << 24) | (uint32_t(plsize) << 29);  // TXEN=0
  write_sfr_(REG_CiFIFOCON + CIFIFO_OFFSET * 1, rxcon);

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
  ESP_LOGD(TAG, "  DEVID  (0xE14) = 0x%08X  (MCP2518FD expect 0x00000028)", devid);
  ESP_LOGD(TAG, "  OSC    (0xE00) = 0x%08X  (POR expect    0x00000060)", osc_pre);
  ESP_LOGD(TAG, "  CiCON  (0x000) = 0x%08X  (POR expect    0x04980760)", cicon);
  ESP_LOGD(TAG, "  IOCON  (0xE04) = 0x%08X  (POR expect    0x03000000)", iocon);

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

  ESP_LOGD(TAG, "Configuring FIFOs...");
  if (configure_fifos_() != canbus::ERROR_OK) { ESP_LOGE(TAG, "configure_fifos_ failed"); return false; }

  ESP_LOGD(TAG, "Configuring filters...");
  if (configure_filters_() != canbus::ERROR_OK) { ESP_LOGE(TAG, "configure_filters_ failed"); return false; }

  // CiCON: enable TXQ, ISO CRC (FD), disable BRS (classic)
  uint32_t con = read_sfr_(REG_CiCON);
  ESP_LOGD(TAG, "CiCON before=0x%08X", con);
  con |= CiCON_TXQEN;
  if (this->canfd_enabled_)
    con |= CiCON_ISOCRCEN;
  else
    con |= CiCON_BRSDIS;
  write_sfr_(REG_CiCON, con);

  ESP_LOGD(TAG, "Configuring interrupts...");
  if (configure_interrupts_() != canbus::ERROR_OK) { ESP_LOGE(TAG, "configure_interrupts_ failed"); return false; }

  ESP_LOGD(TAG, "Entering operational mode %d...", this->mcp_mode_);
  if (set_mode_(static_cast<CanOperationMode>(this->mcp_mode_)) != canbus::ERROR_OK) {
    ESP_LOGE(TAG, "set_mode_(operational) failed");
    return false;
  }

  this->init_citrec_ = read_sfr_(REG_CiTREC);
  this->init_done_ = true;
  ESP_LOGD(TAG, "MCP2518FD ready — CiBDIAG0=0x%08X CiTREC=0x%08X",
           read_sfr_(REG_CiBDIAG0), this->init_citrec_);
  return true;
}



// ============================================================
// dump_config — called after setup, visible via WiFi/API logs
// ============================================================

void MCP2518FD::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP2518FD:");
  ESP_LOGCONFIG(TAG, "  Init success : %s", this->init_done_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  DEVID  (0xE14): 0x%08X (MCP2518FD=0x28)", this->init_devid_);
  ESP_LOGCONFIG(TAG, "  OSC    (0xE00): 0x%08X (POR=0x60)",        this->init_osc_);
  ESP_LOGCONFIG(TAG, "  CiCON  (0x000): 0x%08X (POR=0x04980760)", this->init_cicon_);
  ESP_LOGCONFIG(TAG, "  IOCON  (0xE04): 0x%08X (POR=0x03000000)", this->init_iocon_);
  ESP_LOGCONFIG(TAG, "  CiTREC (0x034): 0x%08X",                   this->init_citrec_);
  LOG_PIN("  CS Pin : ", this->cs_);
  if (!this->init_done_) {
    if (this->init_devid_ == 0x00000000 || this->init_devid_ == 0xFFFFFFFF)
      ESP_LOGE(TAG, "  => SPI communication failed (DEVID=0x%08X)", this->init_devid_);
    else if ((this->init_osc_ & OSC_OSCRDY) == 0)
      ESP_LOGE(TAG, "  => Oscillator not ready (OSC=0x%08X, OSCRDY bit 10 not set)", this->init_osc_);
    else
      ESP_LOGE(TAG, "  => Init failed at unknown step");
  }
}

// ============================================================
// TX
// ============================================================

uint16_t MCP2518FD::tx_fifo_addr_() {
  return static_cast<uint16_t>(read_sfr_(REG_CiTXQUA));
}

canbus::Error MCP2518FD::send_message_txq_(struct canbus::CanFrame *frame) {
  uint8_t dlc = frame->can_data_length_code;
  if (!this->canfd_enabled_ && dlc > canbus::CAN_MAX_DATA_LENGTH)
    return canbus::ERROR_FAILTX;
  if (dlc > CAN_FD_MAX_DATA_LENGTH)
    return canbus::ERROR_FAILTX;

  // Check TXQ not full
  if ((read_sfr_(REG_CiTXQSTA) & TXQSTA_TXQNF) == 0)
    return canbus::ERROR_ALLTXBUSY;

  // CiTXQUA already contains the full SPI address (includes RAM base 0x400)
  uint16_t ram_addr = tx_fifo_addr_();

  // Build transmit message object (T0 + T1 + data)
  // T0: ID word — layout per datasheet Table 3-5 (page 66):
  //   Standard: SID[10:0] at T0[10:0]
  //   Extended: SID[10:0] at T0[10:0], EID[17:0] at T0[28:11]
  uint32_t id_word = 0;
  if (frame->use_extended_id) {
    uint32_t sid = (frame->can_id >> 18) & 0x7FFU;   // bits 28:18 of 29-bit ID
    uint32_t eid =  frame->can_id & 0x3FFFFUL;        // bits 17:0  of 29-bit ID
    id_word = sid | (eid << MSGOBJ_EID_SHIFT);
  } else {
    id_word = frame->can_id & 0x7FFU;                 // SID[10:0] at T0[10:0]
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

  // Set UINC + TXREQ
  uint32_t txqcon = read_sfr_(REG_CiTXQCON);
  txqcon |= TXQCON_UINC | TXQCON_TXREQ;
  write_sfr_(REG_CiTXQCON, txqcon);

  // Brief wait then check TX result diagnostics
  delay(10);
  uint32_t trec   = read_sfr_(REG_CiTREC);
  uint32_t bdiag1 = read_sfr_(REG_CiBDIAG1);
  uint32_t txqsta = read_sfr_(REG_CiTXQSTA);
  ESP_LOGD(TAG, "TX diag: TREC=0x%08X BDIAG1=0x%08X TXQSTA=0x%08X", trec, bdiag1, txqsta);
  if (bdiag1 & (1UL << 18))
    ESP_LOGW(TAG, "  NACKERR: no ACK received — check bus wiring and termination");
  if (trec & (1UL << 21))
    ESP_LOGW(TAG, "  TXBO: bus-off! TEC > 255");
  if (trec & (1UL << 20))
    ESP_LOGW(TAG, "  TXBP: error-passive (TEC > 127)");

  return canbus::ERROR_OK;
}

canbus::Error MCP2518FD::send_message(struct canbus::CanFrame *frame) {
  return send_message_txq_(frame);
}

// ============================================================
// RX
// ============================================================

uint16_t MCP2518FD::rx_fifo_addr_() {
  return static_cast<uint16_t>(read_sfr_(REG_CiFIFOUA + CIFIFO_OFFSET * 1));
}

bool MCP2518FD::rx_available_() {
  // Fast path: if INT1 is wired, check the pin (active-low → asserted = LOW)
  if (this->int1_pin_ != nullptr)
    return !this->int1_pin_->digital_read();  // LOW = interrupt asserted

  // Fallback: read FIFO status register directly
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
    uint32_t sid = id_word & MSGOBJ_SID_MASK;                       // T0[10:0]
    uint32_t eid = (id_word & MSGOBJ_EID_MASK) >> MSGOBJ_EID_SHIFT; // T0[28:11]
    frame->can_id = (sid << 18) | eid;
  } else {
    frame->can_id = id_word & MSGOBJ_SID_MASK;                      // T0[10:0]
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
  return canbus::ERROR_OK;
}

canbus::Error MCP2518FD::read_message(struct canbus::CanFrame *frame) {
  return read_message_fifo_(frame);
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
