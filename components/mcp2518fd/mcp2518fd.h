#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "mcp2518fd_defs.h"

namespace esphome::mcp2518fd {

enum CanClock : uint8_t {
  MCP_40MHZ = 0,
  MCP_20MHZ = 1,
  MCP_10MHZ = 2,
  MCP_4MHZ  = 3,
};

enum CanMode : uint8_t {
  CAN_MODE_NORMAL       = CAN_NORMAL_MODE,
  CAN_MODE_LISTEN_ONLY  = CAN_LISTEN_ONLY_MODE,
  CAN_MODE_LOOPBACK_INT = CAN_INTERNAL_LOOPBACK,
  CAN_MODE_LOOPBACK_EXT = CAN_EXTERNAL_LOOPBACK,
  CAN_MODE_SLEEP        = CAN_SLEEP_MODE,
  CAN_MODE_CLASSIC      = CAN_CLASSIC_MODE,
  CAN_MODE_RESTRICTED   = CAN_RESTRICTED_MODE,
};

class MCP2518FD : public canbus::Canbus,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST,
                                        spi::CLOCK_POLARITY_LOW,
                                        spi::CLOCK_PHASE_LEADING,
                                        spi::DATA_RATE_8MHZ> {
 public:
  MCP2518FD() = default;

  // ---- YAML setters --------------------------------------------------------
  void set_can_clock(CanClock clock)       { this->can_clock_     = clock;   }
  void set_mcp_mode(CanMode mode)          { this->mcp_mode_      = mode;    }
  void set_canfd_enabled(bool enabled)     { this->canfd_enabled_ = enabled; }
  void set_data_rate(canbus::CanSpeed rate){ this->data_rate_      = rate;    }

  // Interrupt pins (all optional, active-low)
  // INT  : general interrupt — any enabled source in CiINT
  // INT0 : TX interrupt     — CiINT.TXIF & TXIE  (INT0/GPIO0 pin)
  // INT1 : RX interrupt     — CiINT.RXIF & RXIE  (INT1/GPIO1 pin)
  void set_int_pin(GPIOPin *pin)  { this->int_pin_  = pin; }
  void set_int0_pin(GPIOPin *pin) { this->int0_pin_ = pin; }
  void set_int1_pin(GPIOPin *pin) { this->int1_pin_ = pin; }

 protected:
  // ---- Component lifecycle -------------------------------------------------
  bool setup_internal() override;

  // ---- canbus::Canbus interface --------------------------------------------
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

 private:
  // ---- Configuration -------------------------------------------------------
  CanClock         can_clock_     {MCP_40MHZ};
  CanMode          mcp_mode_      {CAN_MODE_NORMAL};
  bool             canfd_enabled_ {false};
  canbus::CanSpeed data_rate_     {canbus::CAN_500KBPS};

  // ---- Interrupt pins (nullptr = not configured, use polling) --------------
  GPIOPin *int_pin_  {nullptr};  ///< INT  — general (active-low)
  GPIOPin *int0_pin_ {nullptr};  ///< INT0 — TX done  (active-low)
  GPIOPin *int1_pin_ {nullptr};  ///< INT1 — RX avail (active-low)

  // ---- SPI primitives ------------------------------------------------------
  canbus::Error reset_();
  uint32_t      read_sfr_(uint16_t addr);
  void          write_sfr_(uint16_t addr, uint32_t value);
  uint8_t       read_byte_(uint16_t addr);
  void          write_byte_(uint16_t addr, uint8_t value);
  void          read_ram_(uint16_t addr, uint8_t *data, uint8_t n);
  void          write_ram_(uint16_t addr, const uint8_t *data, uint8_t n);

  // ---- Init helpers --------------------------------------------------------
  canbus::Error configure_osc_();
  canbus::Error set_mode_(CanOperationMode mode);
  canbus::Error configure_bit_timing_();
  canbus::Error configure_fifos_();
  canbus::Error configure_filters_();
  canbus::Error configure_interrupts_();

  // ---- TX / RX helpers -----------------------------------------------------
  canbus::Error send_message_txq_(struct canbus::CanFrame *frame);
  canbus::Error read_message_fifo_(struct canbus::CanFrame *frame);

  uint16_t tx_fifo_addr_();
  uint16_t rx_fifo_addr_();

  // ---- Receive availability ------------------------------------------------
  // Returns true when an RX frame is waiting.
  // Honours INT1 if wired, otherwise reads the FIFO status register directly.
  bool rx_available_();

  // ---- SPI command builder -------------------------------------------------
  inline void build_cmd_(uint16_t addr, uint8_t instr, uint8_t out[2]) {
    out[0] = static_cast<uint8_t>((instr << 4) | ((addr >> 8) & 0x0F));
    out[1] = static_cast<uint8_t>(addr & 0xFF);
  }

  // ---- Utility -------------------------------------------------------------
  static uint8_t dlc_to_bytes_(uint8_t dlc);
  static uint8_t bytes_to_dlc_(uint8_t len);

  bool calc_nbt_(canbus::CanSpeed speed, CanClock clk, uint32_t &nbtcfg) const;
  bool calc_dbt_(canbus::CanSpeed speed, CanClock clk,
                 uint32_t &dbtcfg, uint32_t &tdcval) const;
};

}  // namespace esphome::mcp2518fd
