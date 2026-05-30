#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "mcp2518fd_defs.h"

// ============================================================
// ESPHome native component for the Microchip MCP2518FD
// CAN FD controller with SPI interface.
//
// Supports:
//   - CAN 2.0B  (classic frames up to 8 bytes)
//   - CAN FD    (flexible data-rate frames up to 64 bytes)
//   - Same YAML fields as the mcp2515 component
//
// References:
//   Microchip DS20006027B – MCP2518FD Datasheet
//   esphome/components/mcp2515  (design template)
// ============================================================

namespace esphome::mcp2518fd {

// Oscillator frequency selection (crystal / oscillator on OSC1/OSC2)
enum CanClock : uint8_t {
  MCP_40MHZ = 0,  ///< 40 MHz oscillator (recommended)
  MCP_20MHZ = 1,  ///< 20 MHz oscillator
  MCP_10MHZ = 2,  ///< 10 MHz oscillator
  MCP_4MHZ  = 3,  ///< 4 MHz oscillator  (uses 10× PLL → 40 MHz system clock)
};

// Operating mode exposed to YAML (same names as mcp2515 where possible)
enum CanMode : uint8_t {
  CAN_MODE_NORMAL       = CAN_NORMAL_MODE,
  CAN_MODE_LISTEN_ONLY  = CAN_LISTEN_ONLY_MODE,
  CAN_MODE_LOOPBACK_INT = CAN_INTERNAL_LOOPBACK,
  CAN_MODE_LOOPBACK_EXT = CAN_EXTERNAL_LOOPBACK,
  CAN_MODE_SLEEP        = CAN_SLEEP_MODE,
  CAN_MODE_CLASSIC      = CAN_CLASSIC_MODE,
  CAN_MODE_RESTRICTED   = CAN_RESTRICTED_MODE,
};

// ============================================================
// MCP2518FD  —  main class
// ============================================================
class MCP2518FD : public canbus::Canbus,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST,
                                        spi::CLOCK_POLARITY_LOW,
                                        spi::CLOCK_PHASE_LEADING,
                                        spi::DATA_RATE_8MHZ> {
 public:
  MCP2518FD() = default;

  // ---- YAML setters (match mcp2515 naming as closely as possible) ----------
  void set_mcp_clock(CanClock clock) { this->mcp_clock_ = clock; }
  void set_mcp_mode(CanMode mode)    { this->mcp_mode_  = mode;  }

  // Enable CAN FD mode (if false, device operates as classic CAN 2.0B)
  void set_canfd_enabled(bool enabled) { this->canfd_enabled_ = enabled; }

  // Data-phase bit rate for CAN FD (only relevant when canfd_enabled = true)
  // Uses the same canbus::CanSpeed enum; select the data-phase speed
  void set_data_rate(canbus::CanSpeed rate) { this->data_rate_ = rate; }

 protected:
  // ---- Component lifecycle -------------------------------------------------
  bool setup_internal() override;

  // ---- canbus::Canbus interface --------------------------------------------
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

 private:
  // ---- Configuration -------------------------------------------------------
  CanClock         mcp_clock_     {MCP_40MHZ};
  CanMode          mcp_mode_      {CAN_MODE_NORMAL};
  bool             canfd_enabled_ {false};
  canbus::CanSpeed data_rate_     {canbus::CAN_500KBPS};

  // ---- SPI helpers ---------------------------------------------------------
  canbus::Error reset_();
  uint32_t      read_sfr_(uint16_t addr);
  void          write_sfr_(uint16_t addr, uint32_t value);
  uint8_t       read_byte_(uint16_t addr);
  void          write_byte_(uint16_t addr, uint8_t value);
  void          read_ram_(uint16_t addr, uint8_t *data, uint8_t n);
  void          write_ram_(uint16_t addr, const uint8_t *data, uint8_t n);

  // ---- Initialisation helpers ----------------------------------------------
  canbus::Error configure_osc_();
  canbus::Error set_mode_(CanOperationMode mode);
  canbus::Error configure_bit_timing_();
  canbus::Error configure_fifos_();
  canbus::Error configure_filters_();

  // ---- TX / RX helpers -----------------------------------------------------
  canbus::Error send_message_txq_(struct canbus::CanFrame *frame);
  canbus::Error read_message_fifo_(struct canbus::CanFrame *frame);

  bool check_receive_();
  bool check_error_();

  uint16_t tx_fifo_addr_();
  uint16_t rx_fifo_addr_();

  // ---- SPI command builder -------------------------------------------------
  // MCP2518FD SPI header: [CMD(3:0)|ADDR(11:8), ADDR(7:0)]
  inline void build_cmd_(uint16_t addr, uint8_t instr, uint8_t cmd_out[2]) {
    cmd_out[0] = static_cast<uint8_t>((instr << 4) | ((addr >> 8) & 0x0F));
    cmd_out[1] = static_cast<uint8_t>(addr & 0xFF);
  }

  // ---- Utility -------------------------------------------------------------
  static uint8_t dlc_to_bytes_(uint8_t dlc);
  static uint8_t bytes_to_dlc_(uint8_t len);

  bool calc_nbt_(canbus::CanSpeed speed, CanClock clk, uint32_t &nbtcfg) const;
  bool calc_dbt_(canbus::CanSpeed speed, CanClock clk,
                 uint32_t &dbtcfg, uint32_t &tdcval) const;

  // Internal CAN FD frame buffer (64 bytes data) — allocated on stack in
  // read_message_fifo_ to avoid heap fragmentation.
  // The public API CanFrame only holds 8 bytes; for CAN FD we copy the first
  // min(8, nbytes) bytes so the rest of the ESPHome pipeline stays compatible.
  // Full 64-byte FD payloads are available via the add_callback() raw API.
  uint8_t fd_rx_buf_[CAN_FD_MAX_DATA_LENGTH]{};
};

}  // namespace esphome::mcp2518fd
