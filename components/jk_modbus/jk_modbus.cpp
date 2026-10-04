#include "jk_modbus.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <algorithm>

namespace esphome::jk_modbus {

static const char *const TAG = "jk_modbus";

static const uint8_t FUNCTION_WRITE_REGISTER = 0x02;
static const uint8_t FUNCTION_READ_REGISTER = 0x03;
static const uint8_t FUNCTION_PASSWORD = 0x05;
static const uint8_t FUNCTION_READ_ALL_REGISTERS = 0x06;

static const uint8_t ADDRESS_READ_ALL = 0x00;

static const uint8_t FRAME_SOURCE_GPS = 0x02;

// data_len covers everything but the two CRC bytes. The payload is handed over as
// [11, data_len - 3), so anything below 14 would produce an invalid iterator range.
static const uint16_t MIN_DATA_LEN = 14;
// The largest known status frame (24 cells) has a data_len of ~309; keep some headroom.
static const uint16_t MAX_DATA_LEN = 512;
// Initial capacity of the RX buffers
static const size_t TYPICAL_FRAME_SIZE = 320;
// Time the BMS needs to process the password frame before it accepts a write.
static const uint32_t WRITE_AUTH_DELAY_MS = 150;
// Gap between two queued writes so that authenticate/write pairs never interleave.
static const uint32_t WRITE_GUARD_MS = 10;
// Bytes pulled from the UART per read_array() call.
static const size_t READ_CHUNK_SIZE = 64;

void JkModbus::setup() {
  if (this->flow_control_pin_ != nullptr) {
    this->flow_control_pin_->setup();
  }

  // Avoid the repeated reallocations push_back() would cause while a frame is assembled
  // (the largest known frame, 24 cells, is ~311 bytes)
  this->rx_buffer_.reserve(TYPICAL_FRAME_SIZE);
  this->frame_data_.reserve(TYPICAL_FRAME_SIZE);
}

void JkModbus::loop() {
  const uint32_t now = millis();
  if (now - this->last_jk_modbus_byte_ > this->rx_timeout_) {
    // .data() instead of &front(): front() on an empty vector is undefined behaviour
    ESP_LOGVV(TAG, "Buffer cleared due to timeout: %s",
              format_hex_pretty(this->rx_buffer_.data(), this->rx_buffer_.size()).c_str());  // NOLINT
    this->rx_buffer_.clear();
    this->last_jk_modbus_byte_ = now;
  }

  // Read the UART in chunks: one available() + read_array() per chunk instead of one
  // available() + read_byte() round trip into the UART driver for every single byte.
  uint8_t chunk[READ_CHUNK_SIZE];
  int available;
  while ((available = this->available()) > 0) {
    const size_t len = std::min((size_t) available, sizeof(chunk));
    if (!this->read_array(chunk, len))
      break;

    for (size_t i = 0; i < len; i++) {
      if (this->parse_jk_modbus_byte_(chunk[i])) {
        this->last_jk_modbus_byte_ = now;
      } else {
        ESP_LOGVV(TAG, "Buffer cleared due to reset: %s",
                  format_hex_pretty(this->rx_buffer_.data(), this->rx_buffer_.size()).c_str());  // NOLINT
        this->rx_buffer_.clear();
      }
    }
  }
}

uint16_t chksum(const uint8_t data[], const uint16_t len) {
  uint16_t checksum = 0;
  for (uint16_t i = 0; i < len; i++) {
    checksum = checksum + data[i];
  }
  return checksum;
}

bool JkModbus::parse_jk_modbus_byte_(uint8_t byte) {
  size_t at = this->rx_buffer_.size();
  this->rx_buffer_.push_back(byte);
  const uint8_t *raw = &this->rx_buffer_[0];

  // Byte 0: Start sequence (0x4E)
  if (at == 0) {
    // return false to reset buffer
    return raw[0] == 0x4E;
  }
  uint8_t address = raw[0];

  // Byte 1: Start sequence (0x57)
  if (at == 1) {
    if (raw[0] != 0x4E || raw[1] != 0x57) {
      ESP_LOGW(TAG, "Invalid header: 0x%02X 0x%02X", raw[0], raw[1]);

      // return false to reset buffer
      return false;
    }

    return true;
  }

  // Byte 2: Size (low byte)
  if (at == 2)
    return true;

  // Byte 3: Size (high byte)
  if (at == 3)
    return true;
  uint16_t data_len = (uint16_t(raw[2]) << 8 | (uint16_t(raw[2 + 1]) << 0));

  // Reject implausible lengths as soon as they are known instead of waiting for up to 64 kB of
  // data (or building an invalid iterator range for the payload below).
  if (at == 4 && (data_len < MIN_DATA_LEN || data_len > MAX_DATA_LEN)) {
    ESP_LOGW(TAG, "Invalid frame length: %u", data_len);

    // return false to reset buffer
    return false;
  }

  // data_len: CRC_LO (over all bytes)
  if (at <= data_len)
    return true;

  uint8_t function = raw[8];

  // data_len+1: CRC_HI (over all bytes)
  uint16_t computed_crc = chksum(raw, data_len);
  uint16_t remote_crc = uint16_t(raw[data_len]) << 8 | (uint16_t(raw[data_len + 1]) << 0);
  if (computed_crc != remote_crc) {
    ESP_LOGW(TAG, "CRC check failed! 0x%04X != 0x%04X", computed_crc, remote_crc);
    return false;
  }

  // assign() reuses the capacity of frame_data_, so no heap allocation per frame
  this->frame_data_.assign(this->rx_buffer_.begin() + 11, this->rx_buffer_.begin() + data_len - 3);

  bool found = false;
  for (auto *device : this->devices_) {
    if (device->address_ == address) {
      device->on_jk_modbus_data(function, this->frame_data_);
      found = true;
    }
  }
  if (!found) {
    ESP_LOGW(TAG, "Got JkModbus frame from unknown address 0x%02X!", address);
  }

  // return false to reset buffer
  return false;
}

void JkModbus::dump_config() {
  ESP_LOGCONFIG(TAG, "JkModbus:");
  ESP_LOGCONFIG(TAG, "  RX timeout: %d ms", this->rx_timeout_);
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
}
float JkModbus::get_setup_priority() const {
  // After UART bus
  return setup_priority::BUS - 1.0f;
}

void JkModbus::send(uint8_t function, uint8_t address, uint8_t value) {
  uint8_t frame[22];
  frame[0] = 0x4E;      // start sequence
  frame[1] = 0x57;      // start sequence
  frame[2] = 0x00;      // data length lb
  frame[3] = 0x14;      // data length hb
  frame[4] = 0x00;      // bms terminal number
  frame[5] = 0x00;      // bms terminal number
  frame[6] = 0x00;      // bms terminal number
  frame[7] = 0x00;      // bms terminal number
  frame[8] = function;  // command word: 0x01 (activation), 0x02 (write), 0x03 (read), 0x05 (password), 0x06 (read all)
  frame[9] = FRAME_SOURCE_GPS;  // frame source: 0x00 (bms), 0x01 (bluetooth), 0x02 (gps), 0x03 (computer)
  frame[10] = 0x00;             // frame type: 0x00 (read data), 0x01 (reply frame), 0x02 (BMS active upload)
  frame[11] = address;          // register: 0x00 (read all registers), 0x8E...0xBF (holding registers)
  frame[12] = value;            // data
  frame[13] = 0x00;             // record number
  frame[14] = 0x00;             // record number
  frame[15] = 0x00;             // record number
  frame[16] = 0x00;             // record number
  frame[17] = 0x68;             // end sequence
  auto crc = chksum(frame, 18);
  frame[18] = 0x00;  // crc unused
  frame[19] = 0x00;  // crc unused
  frame[20] = crc >> 8;
  frame[21] = crc >> 0;

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(true);

  this->write_array(frame, 22);
  this->flush();

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(false);
}

void JkModbus::authenticate_() { this->send(FUNCTION_PASSWORD, 0x00, 0x00); }

void JkModbus::write_register(uint8_t address, uint8_t value) {
  // authenticate -> wait -> write, scheduled instead of delay(150) so the main loop (WiFi, API,
  // UART RX) keeps running. Writes are serialized: a second write waits until the first pair is done.
  uint32_t wait = 0;
  if (this->write_busy_) {
    const int32_t remaining = (int32_t) (this->write_busy_until_ - millis());
    if (remaining > 0)
      wait = (uint32_t) remaining;
  }

  const uint32_t busy_for = wait + WRITE_AUTH_DELAY_MS + WRITE_GUARD_MS;

  this->set_timeout(wait, [this]() { this->authenticate_(); });
  this->set_timeout(wait + WRITE_AUTH_DELAY_MS,
                    [this, address, value]() { this->send(FUNCTION_WRITE_REGISTER, address, value); });
  // The flag is cleared by the last timeout, so a stale write_busy_until_ is never compared
  // against millis() after the 32 bit counter wrapped.
  this->set_timeout(busy_for, [this]() { this->write_busy_ = false; });

  this->write_busy_ = true;
  this->write_busy_until_ = millis() + busy_for;
}

void JkModbus::read_registers() {
  // Don't talk over a response that is still being received (half-duplex RS485): the request
  // would collide with the BMS transmission. Matters most with a short update_interval.
  if (!this->rx_buffer_.empty() && (millis() - this->last_jk_modbus_byte_) <= this->rx_timeout_) {
    ESP_LOGD(TAG, "Skipping request: a response is still being received");
    return;
  }

  uint8_t frame[21];
  frame[0] = 0x4E;                         // start sequence
  frame[1] = 0x57;                         // start sequence
  frame[2] = 0x00;                         // data length lb
  frame[3] = 0x13;                         // data length hb
  frame[4] = 0x00;                         // bms terminal number
  frame[5] = 0x00;                         // bms terminal number
  frame[6] = 0x00;                         // bms terminal number
  frame[7] = 0x00;                         // bms terminal number
  frame[8] = FUNCTION_READ_ALL_REGISTERS;  // command word: 0x01 (activation), 0x02 (write), 0x03 (read), 0x05
                                           // (password), 0x06 (read all)
  frame[9] = FRAME_SOURCE_GPS;             // frame source: 0x00 (bms), 0x01 (bluetooth), 0x02 (gps), 0x03 (computer)
  frame[10] = 0x00;                        // frame type: 0x00 (read data), 0x01 (reply frame), 0x02 (BMS active upload)
  frame[11] = ADDRESS_READ_ALL;            // register: 0x00 (read all registers), 0x8E...0xBF (holding registers)
  frame[12] = 0x00;                        // record number
  frame[13] = 0x00;                        // record number
  frame[14] = 0x00;                        // record number
  frame[15] = 0x00;                        // record number
  frame[16] = 0x68;                        // end sequence
  auto crc = chksum(frame, 17);
  frame[17] = 0x00;  // crc unused
  frame[18] = 0x00;  // crc unused
  frame[19] = crc >> 8;
  frame[20] = crc >> 0;

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(true);

  this->write_array(frame, 21);
  this->flush();

  if (this->flow_control_pin_ != nullptr)
    this->flow_control_pin_->digital_write(false);
}

}  // namespace esphome::jk_modbus
