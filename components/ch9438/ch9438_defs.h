/// @file ch9438_defs.h
/// @brief WCH CH9438 (SPI to 8 UART + 8 GPIO) - register and constant definitions
/// @details Sources: CH9438 datasheet V1.1 and the register/protocol facts observed in the vendor Linux driver
/// (facts only, no vendor code is used here).

#pragma once
#include <cstddef>
#include <cstdint>

namespace esphome::ch9438 {

// ---------------------------------------------------------------------------------------------------------------------
// SPI transport
// ---------------------------------------------------------------------------------------------------------------------
constexpr uint8_t CMD_WRITE_FLAG = 0x80;   ///< bit 7 of the address byte: 1 = write, 0 = read
constexpr uint32_t CMD_DELAY_US = 3;       ///< delay between the address byte and the data bytes (datasheet: 2 us write,
                                           ///< 3 us read; 3 us is used everywhere)
constexpr uint32_t BULK_DELAY_US = 9;      ///< delay after the address byte of the (undocumented) bulk status read
constexpr uint32_t GAP_US = 3;             ///< minimum idle time between two chip selects
constexpr size_t XFER_MAX_SIZE = 512;      ///< maximum number of bytes moved by one FIFO burst
constexpr uint8_t SPI_PROBE_RETRIES = 3;   ///< number of attempts for the hardware and version tests

// ---------------------------------------------------------------------------------------------------------------------
// Per UART registers: SPI address = channel * 8 + register
// ---------------------------------------------------------------------------------------------------------------------
constexpr uint8_t UART_RBR = 0x00;  ///< receive FIFO (read)
constexpr uint8_t UART_THR = 0x00;  ///< transmit FIFO (write)
constexpr uint8_t UART_DLL = 0x00;  ///< divisor latch low  (LCR.DLAB = 1)
constexpr uint8_t UART_DLM = 0x01;  ///< divisor latch high (LCR.DLAB = 1)
constexpr uint8_t UART_IER = 0x01;
constexpr uint8_t UART_IIR = 0x02;
constexpr uint8_t UART_FCR = 0x02;
constexpr uint8_t UART_LCR = 0x03;
constexpr uint8_t UART_MCR = 0x04;
constexpr uint8_t UART_LSR = 0x05;
constexpr uint8_t UART_MSR = 0x06;
constexpr uint8_t UART_SPR = 0x07;

constexpr uint8_t IER_RDI = 1 << 0;   ///< receive data interrupt
constexpr uint8_t IER_THRI = 1 << 1;  ///< transmit holding register interrupt (unused: TX is polled)
constexpr uint8_t IER_RLSI = 1 << 2;  ///< receive line status interrupt
constexpr uint8_t IER_MSI = 1 << 3;   ///< modem status interrupt (unused)

constexpr uint8_t FCR_FIFOEN = 1 << 0;
constexpr uint8_t FCR_RXRST = 1 << 1;
constexpr uint8_t FCR_TXRST = 1 << 2;
constexpr uint32_t FCR_SETTLE_MS = 2;  ///< the vendor driver waits 2 ms after writing the FCR

constexpr uint8_t LCR_WORD_8 = 0x03;
constexpr uint8_t LCR_STOP2 = 1 << 2;
constexpr uint8_t LCR_PAREN = 1 << 3;
constexpr uint8_t LCR_PAR_EVEN = 1 << 4;  ///< PARMODE0 (0 = odd, 1 = even)
constexpr uint8_t LCR_DLAB = 1 << 7;

constexpr uint8_t MCR_RTS = 1 << 1;
constexpr uint8_t MCR_AFE = 1 << 5;  ///< automatic hardware flow control (RTS/CTS)

constexpr uint8_t LSR_DR = 1 << 0;
constexpr uint8_t LSR_OE = 1 << 1;
constexpr uint8_t LSR_PE = 1 << 2;
constexpr uint8_t LSR_FE = 1 << 3;
constexpr uint8_t LSR_BI = 1 << 4;
constexpr uint8_t LSR_THRE = 1 << 5;
constexpr uint8_t LSR_TEMT = 1 << 6;
constexpr uint8_t LSR_ERROR_MASK = LSR_OE | LSR_PE | LSR_FE | LSR_BI;

// ---------------------------------------------------------------------------------------------------------------------
// Interface registers
// ---------------------------------------------------------------------------------------------------------------------
constexpr uint8_t REG_TNOW_CTRL1 = 0x41;   ///< TNOW0-3: [3:0] enable, [7:4] invert polarity
constexpr uint8_t REG_FIFO_CTRL = 0x42;    ///< FIFO counter selection: bit 4 = direction (1 = TX), [3:0] = UART
constexpr uint8_t REG_FIFO_CNT_L = 0x43;
constexpr uint8_t REG_FIFO_CNT_H = 0x44;
constexpr uint8_t REG_IO_SEL_FUN_CFG = 0x45;  ///< pin function multiplexing (4 byte access)
constexpr uint8_t REG_TNOW_CTRL2 = 0x47;      ///< TNOW4-7: [3:0] enable, [7:4] invert polarity
constexpr uint8_t REG_CLK_CTRL = 0x48;        ///< [7:6]: x0 internal clock, x1 external crystal
constexpr uint8_t REG_SLEEP_MOD = 0x4A;
constexpr uint8_t REG_BULK = 0x4C;  ///< NOT in the datasheet: one read returns IIR/LSR/MSR/RX count/TX count of all UARTs
constexpr uint8_t REG_GPIO_FUNC_EN = 0x50;
constexpr uint8_t REG_GPIO_DIR_MOD0 = 0x54;  ///< 0x54..0x57, 4 bits per GPIO
constexpr uint8_t REG_GPIO_SET = 0x5C;
constexpr uint8_t REG_GPIO_RESET = 0x5E;
constexpr uint8_t REG_GPIO_PIN_VAL = 0x60;
constexpr uint8_t REG_CHIP_VER = 0x65;  ///< 4 bytes: VER0, VER1, VER2 = VER0 + VER1, VER3 = 0x7C

constexpr uint8_t CHIP_VER3_ID = 0x7C;
constexpr uint8_t FIFO_DIR_RX = 0 << 4;
constexpr uint8_t FIFO_DIR_TX = 1 << 4;

constexpr uint8_t CLK_INTERNAL = 0x00;
constexpr uint8_t CLK_EXTERNAL = 0x03 << 6;
constexpr uint32_t CLK_SETTLE_MS = 200;  ///< the vendor driver waits 200 ms after writing the clock register

// GPIO direction / mode nibble: [3:2] = config, [1:0] = direction
constexpr uint8_t GPIO_DIR_INPUT = 0x0;
constexpr uint8_t GPIO_DIR_OUTPUT = 0x3;
constexpr uint8_t GPIO_CFG_IN_FLOATING = 0x1 << 2;
constexpr uint8_t GPIO_CFG_IN_PULL = 0x2 << 2;
constexpr uint8_t GPIO_CFG_OUT_PUSHPULL = 0x0 << 2;
constexpr uint8_t GPIO_CFG_OUT_OPENDRAIN = 0x1 << 2;

// ---------------------------------------------------------------------------------------------------------------------
// Pin function multiplexing (REG_IO_SEL_FUN_CFG): [command][pin address][enable][0xA5] -> [command][pin address][..][0x5A]
// ---------------------------------------------------------------------------------------------------------------------
constexpr uint8_t IO_CMD_MULTI_SET = 0x01;
constexpr uint8_t IO_CMD_MULTI_GET = 0x81;
constexpr uint8_t IO_CMD_DEFAULT_SET = 0x03;
constexpr uint8_t IO_CMD_DEFAULT_GET = 0x83;
constexpr uint8_t IO_ACT = 0xA5;
constexpr uint8_t IO_DONE = 0x5A;

constexpr uint8_t IO_ADDR_INT = 1;
constexpr uint8_t IO_ADDR_TNOW0 = 2;   ///< TNOW0..7 = 2..9
constexpr uint8_t IO_ADDR_CTS1 = 11;   ///< CTS1..4 = 11..14
constexpr uint8_t IO_ADDR_RTS1 = 19;   ///< RTS1..4 = 19..22
constexpr uint8_t IO_ADDR_GPIO0 = 26;  ///< GPIO0..7 = 26..33
constexpr uint8_t IO_ADDR_UART0 = 1;   ///< default pins: UART0..7 TX/RX = 1..8
constexpr uint8_t IO_ADDR_HSE = 9;     ///< default pins: crystal XI/XO

// ---------------------------------------------------------------------------------------------------------------------
// Chip capabilities
// ---------------------------------------------------------------------------------------------------------------------
constexpr uint8_t NUM_CHANNELS = 8;
constexpr uint8_t NUM_GPIO = 8;
constexpr size_t BULK_BYTES_PER_UART = 7;  ///< IIR, LSR, MSR, RX count L/H, TX count L/H
constexpr size_t BULK_SIZE = BULK_BYTES_PER_UART * NUM_CHANNELS;
constexpr size_t RX_FIFO_SIZE = 2048;
constexpr size_t TX_FIFO_SIZE = 1536;
constexpr uint32_t UART_REF_CLOCK = 192000000;  ///< baud = 192 MHz / divisor, divisor is a 12.4 fixed point value
constexpr uint32_t BAUD_MIN = 2930;             ///< 192 MHz / 65535
constexpr uint32_t BAUD_MAX = 12000000;

}  // namespace esphome::ch9438
