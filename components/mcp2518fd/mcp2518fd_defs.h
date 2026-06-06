#pragma once
#include <cstdint>

// ============================================================
// MCP2518FD — Register definitions & SPI instruction set
// Based on Microchip DS20006027B (MCP2518FD datasheet)
// Modelled after esphome/components/mcp2515/mcp2515_defs.h
// ============================================================

namespace esphome::mcp2518fd {

// ---------------------------------------------------------------------------
// SPI Instructions
// ---------------------------------------------------------------------------
static const uint8_t MCP2518FD_INSTRUCTION_RESET       = 0x00;
static const uint8_t MCP2518FD_INSTRUCTION_READ        = 0x03;
static const uint8_t MCP2518FD_INSTRUCTION_READ_CRC    = 0x0B;
static const uint8_t MCP2518FD_INSTRUCTION_WRITE       = 0x02;
static const uint8_t MCP2518FD_INSTRUCTION_WRITE_CRC   = 0x0A;
static const uint8_t MCP2518FD_INSTRUCTION_WRITE_SAFE  = 0x0C;

// ---------------------------------------------------------------------------
// SFR Register addresses  (16-bit addressing used in SPI command header)
// ---------------------------------------------------------------------------
static const uint16_t REG_CiCON        = 0x000;  // CAN Control
static const uint16_t REG_CiNBTCFG    = 0x004;  // Nominal Bit Time Config
static const uint16_t REG_CiDBTCFG    = 0x008;  // Data Bit Time Config
static const uint16_t REG_CiTDC        = 0x00C;  // Transmitter Delay Compensation
static const uint16_t REG_CiTBC        = 0x010;  // Time Base Counter
static const uint16_t REG_CiTSCON     = 0x014;  // Timestamp Control
static const uint16_t REG_CiVEC        = 0x018;  // Interrupt Vector
static const uint16_t REG_CiINT        = 0x01C;  // Interrupt (flags + enable, 32-bit)
static const uint16_t REG_CiINTFLAG   = 0x01C;  // Interrupt Flags (lower 16 bits)
static const uint16_t REG_CiINTENABLE = 0x01E;  // Interrupt Enable  (upper 16 bits)
static const uint16_t REG_CiRXIF       = 0x020;  // RX Interrupt Flag
static const uint16_t REG_CiTXIF       = 0x024;  // TX Interrupt Flag
static const uint16_t REG_CiRXOVIF    = 0x028;  // RX Overflow Interrupt Flag
static const uint16_t REG_CiTXATIF    = 0x02C;  // TX Attempt Interrupt Flag
static const uint16_t REG_CiTXREQ     = 0x030;  // TX Request
static const uint16_t REG_CiTREC      = 0x034;  // TX/RX Error Count
static const uint16_t REG_CiBDIAG0    = 0x038;  // Bus Diagnostics 0
static const uint16_t REG_CiBDIAG1    = 0x03C;  // Bus Diagnostics 1
static const uint16_t REG_CiTEFCON    = 0x040;  // TEF Control
static const uint16_t REG_CiTEFSTA    = 0x044;  // TEF Status
static const uint16_t REG_CiTEFUA     = 0x048;  // TEF User Address

// TX Queue (FIFO CH0)
static const uint16_t REG_CiTXQCON    = 0x050;
static const uint16_t REG_CiTXQSTA    = 0x054;
static const uint16_t REG_CiTXQUA     = 0x058;

// FIFO registers (CH1 = first RX FIFO, offset 3*4 per channel)
static const uint16_t REG_CiFIFOCON   = 0x050;
static const uint16_t REG_CiFIFOSTA   = 0x054;
static const uint16_t REG_CiFIFOUA    = 0x058;
static const uint16_t CIFIFO_OFFSET   = 12;   // 3 * 4 bytes per FIFO set

// Filters  (start after 32 FIFO blocks)
static const uint16_t REG_CiFLTCON    = REG_CiFIFOCON + (CIFIFO_OFFSET * 32);
static const uint16_t REG_CiFLTOBJ    = REG_CiFLTCON + 32;
static const uint16_t REG_CiMASK      = REG_CiFLTOBJ + 4;

// MCP25xxFD Specific SFRs
static const uint16_t REG_OSC         = 0xE00;  // Oscillator Control
static const uint16_t REG_IOCON       = 0xE04;  // I/O Control
static const uint16_t REG_CRC         = 0xE08;  // CRC
static const uint16_t REG_ECCCON      = 0xE0C;  // ECC Control
static const uint16_t REG_ECCSTA      = 0xE10;  // ECC Status
static const uint16_t REG_DEVID       = 0xE14;  // Device ID (MCP2518FD only)

// RAM
static const uint16_t RAM_ADDR_START  = 0x400;
static const uint16_t RAM_SIZE        = 2048;

// ---------------------------------------------------------------------------
// CiCON — Operating modes
// ---------------------------------------------------------------------------
enum CanOperationMode : uint8_t {
  CAN_NORMAL_MODE           = 0x00,
  CAN_SLEEP_MODE            = 0x01,
  CAN_INTERNAL_LOOPBACK     = 0x02,
  CAN_LISTEN_ONLY_MODE      = 0x03,
  CAN_CONFIGURATION_MODE    = 0x04,
  CAN_EXTERNAL_LOOPBACK     = 0x05,
  CAN_CLASSIC_MODE          = 0x06,
  CAN_RESTRICTED_MODE       = 0x07,
  CAN_INVALID_MODE          = 0xFF,
};

// CiCON bit masks
static const uint32_t CiCON_REQOP_MASK   = 0x07000000UL;  // bits 26:24
static const uint8_t  CiCON_REQOP_SHIFT  = 24;
static const uint32_t CiCON_OPMOD_MASK   = 0x00E00000UL;  // bits 23:21
static const uint8_t  CiCON_OPMOD_SHIFT  = 21;
static const uint32_t CiCON_ABAT         = 1UL << 27;
static const uint32_t CiCON_TXQEN        = 1UL << 20;     // TXQ Enable
static const uint32_t CiCON_STEF         = 1UL << 19;     // Store in TEF
static const uint32_t CiCON_SERR2LOM    = 1UL << 18;     // System Error to Listen-Only
static const uint32_t CiCON_ESIGM        = 1UL << 17;     // ESI in Gateway Mode
static const uint32_t CiCON_RTXAT        = 1UL << 16;     // Restrict Re-Tx Attempts
static const uint32_t CiCON_BRSDIS       = 1UL << 12;     // Bit Rate Switching Disable
static const uint32_t CiCON_WAKFIL       = 1UL <<  8;     // CAN Bus Line Wake-up Filter
static const uint32_t CiCON_PXEDIS       = 1UL <<  6;     // Protocol Exception Disable
static const uint32_t CiCON_ISOCRCEN     = 1UL <<  5;     // ISO CRC Enable

// ---------------------------------------------------------------------------
// CiINTFLAG / CiINTENABLE  (lower/upper 16 bits of REG_CiINT)
// ---------------------------------------------------------------------------
enum CiIntFlag : uint16_t {
  INT_TXIF   = 1 << 0,   // TX FIFO interrupt
  INT_RXIF   = 1 << 1,   // RX FIFO interrupt
  INT_TBCIF  = 1 << 2,   // Time Base Counter overflow
  INT_MODIF  = 1 << 3,   // Operation Mode Change
  INT_TEFIF  = 1 << 4,   // TEF interrupt
  INT_ECCIF  = 1 << 8,   // ECC Error
  INT_SPICRCIF = 1 << 9, // SPI CRC Error
  INT_TXATIF = 1 << 10,  // TX Attempt interrupt
  INT_RXOVIF = 1 << 11,  // RX Object Overflow
  INT_SERRIF = 1 << 12,  // System Error
  INT_CERRIF = 1 << 13,  // CAN Bus Error
  INT_WAKIF  = 1 << 14,  // Bus Wake-Up
  INT_IVMIF  = 1 << 15,  // Invalid Message
};

// ---------------------------------------------------------------------------
// FIFO Control Register (CiFIFOCON)
// ---------------------------------------------------------------------------
static const uint32_t FIFOCON_TXEN      = 1UL <<  7;  // TX FIFO enable
static const uint32_t FIFOCON_RTREN     = 1UL <<  6;  // Auto-RTR transmit
static const uint32_t FIFOCON_TXREQ     = 1UL <<  9;  // TX Request (set to send)
static const uint32_t FIFOCON_UINC      = 1UL << 8;   // Increment head/tail
static const uint32_t FIFOCON_RXTSEN    = 1UL <<  5;  // RX Timestamp enable

// FIFO Status Register (CiFIFOSTA)
static const uint8_t FIFOSTA_RXNOTEMPTY = 1 << 0;
static const uint8_t FIFOSTA_RXHALF     = 1 << 1;
static const uint8_t FIFOSTA_RXFULL     = 1 << 2;
static const uint8_t FIFOSTA_RXOVIF     = 1 << 3;
static const uint8_t FIFOSTA_TXNOTFULL  = 1 << 0;
static const uint8_t FIFOSTA_TXHALF     = 1 << 1;
static const uint8_t FIFOSTA_TXEMPTY    = 1 << 2;

// TX Queue Control / Status
static const uint32_t TXQCON_TXREQ      = 1UL << 9;
static const uint32_t TXQCON_UINC       = 1UL << 8;
static const uint8_t  TXQSTA_TXQNF      = 1 << 0;  // TX Queue Not Full

// ---------------------------------------------------------------------------
// OSC Register (0xE00)  — Oscillator Control
// ---------------------------------------------------------------------------
static const uint32_t OSC_PLLEN         = 1UL << 0;  // PLL Enable
static const uint32_t OSC_OSCDIS        = 1UL << 2;  // Oscillator Disable
static const uint32_t OSC_SCLKDIV       = 1UL << 4;  // SCLK Divider (0=1, 1=2)
static const uint32_t OSC_CLKODIV_MASK  = 0x00000060UL; // CLKO Divider bits 9:8
static const uint8_t  OSC_CLKODIV_SHIFT = 5;
static const uint32_t OSC_PLLRDY        = 1UL << 8;  // PLL Ready (status)
static const uint32_t OSC_OSCRDY        = 1UL << 10;  // OSC Ready (status)
static const uint32_t OSC_SCLKRDY       = 1UL << 12;  // SCLK Ready (status)

// CLKO divider values
enum OscClkoDivide : uint8_t {
  OSC_CLKO_DIV1  = 0,
  OSC_CLKO_DIV2  = 1,
  OSC_CLKO_DIV4  = 2,
  OSC_CLKO_DIV10 = 3,
};

// ---------------------------------------------------------------------------
// CiNBTCFG / CiDBTCFG  — Bit Timing
// ---------------------------------------------------------------------------
// CiNBTCFG: [31:24]=BRP [23:16]=TSEG1 [14:8]=TSEG2 [6:0]=SJW
// CiDBTCFG: [31:24]=BRP [20:16]=TSEG1 [11:8]=TSEG2 [3:0]=SJW

// ---------------------------------------------------------------------------
// CAN FD Message Object  (TX / RX objects in RAM)
// ---------------------------------------------------------------------------
// Each object header = 8 bytes (ID word + CTRL word), optionally +4 timestamp
// Data follows immediately after the header.

// TX/RX message object CTRL bits
static const uint32_t MSGOBJ_DLC_MASK  = 0x0FUL;
static const uint32_t MSGOBJ_IDE       = 1UL <<  4;  // Extended ID
static const uint32_t MSGOBJ_RTR       = 1UL <<  5;  // Remote Frame
static const uint32_t MSGOBJ_BRS       = 1UL <<  6;  // Bit Rate Switch
static const uint32_t MSGOBJ_FDF       = 1UL <<  7;  // FD Frame
static const uint32_t MSGOBJ_ESI       = 1UL <<  8;  // Error Status Indicator

// ID word layout
// EID[17:0] in bits 17:0, SID[10:0] in bits 28:18
static const uint32_t MSGOBJ_SID_SHIFT = 0;   // SID[10:0] at T0[10:0]
static const uint32_t MSGOBJ_EID_SHIFT = 11;  // EID[17:0] at T0[28:11]
static const uint32_t MSGOBJ_SID_MASK  = 0x000007FFUL;  // SID at bits [10:0]
static const uint32_t MSGOBJ_EID_MASK  = 0x1FFFF800UL;  // EID at bits [28:11]

// Filter object / Mask object  (CiFLTOBJ / CiMASK)
static const uint32_t FLTOBJ_EXIDE     = 1UL << 30;  // match extended IDs
static const uint32_t FLTOBJ_SID11     = 1UL << 29;
static const uint32_t MASKOBJ_MIDE     = 1UL << 30;  // filter on IDE bit

// ---------------------------------------------------------------------------
// DLC → data byte count table (CAN FD)
// ---------------------------------------------------------------------------
static const uint8_t DLC_TO_BYTES[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64
};

static const uint8_t CAN_FD_MAX_DATA_LENGTH = 64;

}  // namespace esphome::mcp2518fd
