#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/modbustcp_controller/modbustcp_controller.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace esphome::marstek {

using modbustcp_controller::ModbusRegisterType;
using modbustcp_controller::SensorItem;
using modbustcp_controller::SensorValueType;

// RS485 control mode: the battery only accepts the force / power / SOC commands (registers 42010..42999) once
// this register holds RS485_CONTROL_ENABLE (0x55AA); RS485_CONTROL_DISABLE (0x55BB) gives control back.
static constexpr uint16_t REG_RS485_CONTROL_MODE = 42000;
static constexpr uint16_t RS485_CONTROL_ENABLE = 21930;
static constexpr uint16_t RS485_CONTROL_DISABLE = 21947;
static constexpr uint16_t REG_CONTROLLED_LAST = 42999;

class MarstekHub;

/** Base of every entity that reads a holding register.
 *
 *  The controller may rewrite `start_address` / `offset` when it merges neighbouring items into one read range,
 *  so the register the entity was declared with is kept apart in `reg_` (it is the one used for writes).
 *  `parse_and_publish()` is called with the whole range payload; the entity reads its value at `offset`.
 */
class MarstekRegisterItem : public SensorItem {
 public:
  void configure(uint16_t reg, uint8_t count, SensorValueType vtype, uint16_t skip_updates) {
    this->reg_ = reg;
    this->register_type = ModbusRegisterType::HOLDING;
    this->start_address = reg;
    this->register_count = count;
    this->sensor_value_type = vtype;
    this->skip_updates = skip_updates;
    this->bitmask = 0xFFFFFFFF;
    this->offset = 0;
  }
  uint16_t reg() const { return this->reg_; }

 protected:
  /// True when the payload holds all the registers of this item
  bool has_data_(const std::vector<uint8_t> &data) const {
    return data.size() >= static_cast<size_t>(this->offset) + static_cast<size_t>(this->register_count) * 2u;
  }
  /// Numeric value (16/32 bit, signed or not, according to `sensor_value_type`)
  int64_t raw_(const std::vector<uint8_t> &data) const {
    return modbustcp_controller::payload_to_number(data, this->sensor_value_type, this->offset, this->bitmask);
  }
  /// Register `index` of this item (host order)
  uint16_t word_(const std::vector<uint8_t> &data, size_t index) const {
    const size_t pos = static_cast<size_t>(this->offset) + index * 2u;
    return static_cast<uint16_t>((static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]);
  }

  uint16_t reg_{0};
};

/** Reads a register on behalf of a calculated entity (efficiency, stored energy, firmware version...), so that
 *  those entities do not require the source sensors to be declared in the YAML. Neighbouring registers are still
 *  merged into the same read range as the declared sensors. */
class MarstekDepReader : public MarstekRegisterItem {
 public:
  void set_scale(float scale) { this->scale_ = scale; }
  void set_callback(std::function<void()> &&callback) { this->callback_ = std::move(callback); }
  bool valid() const { return this->valid_; }
  float value() const { return this->value_; }
  void parse_and_publish(const std::vector<uint8_t> &data) override;

 protected:
  float scale_{1.0f};
  float value_{0.0f};
  bool valid_{false};
  std::function<void()> callback_{};
};

/** Marstek Venus battery, Modbus TCP.
 *
 *  Inherits the read ranges, the command queue, the retries and the offline detection of the Modbus TCP
 *  controller. The entities (sensor, number, ...) register themselves with add_sensor_item(); their registers,
 *  types and scales come from the register table of the selected model (see registers.py).
 */
class MarstekHub : public modbustcp_controller::ModbusTCPController {
 public:
  MarstekHub() { this->set_split_ranges_by_skip(true); }

  void setup() override;
  void dump_config() override;
  void on_modbus_data(const std::vector<uint8_t> &data) override;
  void on_modbus_error(uint8_t function_code, uint8_t exception_code) override;

  void set_model(const char *model) { this->model_ = model; }
  void set_auto_rs485_control(bool enable) { this->auto_rs485_control_ = enable; }
  /// Called by the rs485_control_mode switch when it reads the register back
  void set_rs485_enabled(bool enabled) { this->rs485_enabled_ = enabled; }

  /// Queue a "write single register" (FC 0x06). With `auto_rs485_control` the RS485 control mode is enabled first
  /// when a command register (42001..42999) is written and the mode is not known to be active.
  void write_register(uint16_t reg, uint16_t value);

#ifdef USE_BINARY_SENSOR
  void set_connection_sensor(binary_sensor::BinarySensor *sensor) { this->connection_sensor_ = sensor; }
#endif

 protected:
  void set_connected_(bool connected);

  const char *model_{""};
  bool auto_rs485_control_{false};
  bool rs485_enabled_{false};
  bool connected_{false};
  bool connected_known_{false};
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *connection_sensor_{nullptr};
#endif
};

}  // namespace esphome::marstek
