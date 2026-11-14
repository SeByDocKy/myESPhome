#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/modbus_controller/modbus_controller.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace esphome::bluetti {

using modbus::EntityType;
using modbus::helpers::SensorValueType;
using modbus_controller::ModbusController;
using modbus_controller::RangeReuse;
using modbus_controller::SensorItem;
using modbus_controller::WriterEntity;

class BluettiHub;

/** Base of every entity that reads a register.
 *
 *  The controller resolves where the item sits in the read range it builds (`offset`, a byte offset into the
 *  response). `parse_and_publish()` is called with the whole range payload; the entity reads its value at `offset`.
 *  BLUETTI registers are holding registers (function code 03); 32 / 64 bit values come low word first.
 */
class BluettiRegisterItem : public SensorItem {
 public:
  void configure(uint16_t reg, uint8_t count, SensorValueType vtype, EntityType rtype) {
    this->reg_ = reg;
    this->register_type = rtype;
    this->set_address(reg);
    this->set_offset_from_start_address(0);
    this->sensor_value_type = vtype;
    this->bitmask = 0xFFFFFFFF;
    // Strings, flags and hex values (RAW) span `count` registers
    if (vtype == SensorValueType::RAW)
      this->set_register_size(static_cast<uint8_t>(count * 2u));
    this->count_ = count;
  }
  uint16_t reg() const { return this->reg_; }
  uint8_t count() const { return this->count_; }

  /// Write protection: the values read back are ignored for `ms` after a write, so that the entity does not jump
  /// back to its old value while the device is still applying the command.
  void hold_for(uint32_t ms) {
    this->hold_until_ = millis() + ms;
    this->hold_active_ = true;
  }

 protected:
  /// True while the values read are to be ignored (see hold_for())
  bool held_() {
    if (!this->hold_active_)
      return false;
    if (static_cast<int32_t>(millis() - this->hold_until_) >= 0) {
      this->hold_active_ = false;
      return false;
    }
    return true;
  }
  /// True when the payload holds all the registers of this item
  bool has_data_(std::span<const uint8_t> data) const {
    return data.size() >= static_cast<size_t>(this->offset) + static_cast<size_t>(this->count_) * 2u;
  }
  /// Numeric value (16/32/64 bit, signed or not, in the word order of `sensor_value_type`)
  int64_t raw_(std::span<const uint8_t> data) const {
    return modbus::helpers::payload_to_number(data, this->sensor_value_type, this->offset, this->bitmask).value_or(0);
  }
  /// Register `index` of this item (host order)
  uint16_t word_(std::span<const uint8_t> data, size_t index) const {
    const size_t pos = static_cast<size_t>(this->offset) + index * 2u;
    return static_cast<uint16_t>((static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]);
  }

  uint16_t reg_{0};
  uint8_t count_{1};
  uint32_t hold_until_{0};
  bool hold_active_{false};
};

// After a write the device needs a moment to apply the command: the values read back meanwhile may be the old ones
static constexpr uint32_t WRITE_HOLD_MS = 10000;

/// Reads a register for the hub itself (the heartbeat that tells the device answers).
class BluettiDepReader : public BluettiRegisterItem {
 public:
  void set_callback(std::function<void(int64_t)> &&callback) { this->callback_ = std::move(callback); }
  void parse_and_publish(std::span<const uint8_t> data) override {
    if (!this->has_data_(data))
      return;
    if (this->callback_)
      this->callback_(this->raw_(data));
  }

 protected:
  std::function<void(int64_t)> callback_{};
};

/// Write access to the device through the ESPHome Modbus controller (one instance per kind of write, so that the
/// requests of one kind never share a queue slot with another).
class BluettiWriter : public WriterEntity {
 public:
  void init(ModbusController *controller) { this->set_controller_(controller); }
  using WriterEntity::write_single_register;
};

/** BLUETTI Balco series (Balco 260, Balco 500), Modbus.
 *
 *  Sits on top of the ESPHome `modbus` bus and `modbus_controller` (declared in the YAML), whatever the transport
 *  under the bus (Modbus TCP through `tcp_uart` + `modbus_tcp_uart`, or a UART on RS485): the controller builds the
 *  read ranges, sends them and handles the retries and the offline detection. The entities (sensor, text_sensor,
 *  ...) register themselves with add_item(); their registers, types and scales come from the register table (see
 *  registers.py). The slow entities (versions, serial numbers, counters, settings) go to a second controller with a
 *  slower `update_interval` when one is given (`slow_modbus_controller_id`).
 *
 *  The BLUETTI firmware only answers a read that lies inside one of its register blocks and is not longer than
 *  `max_span` registers. The controller would merge neighbouring registers into one request of up to 125 registers,
 *  so the hub cuts the read ranges itself, at the block boundaries and every `max_span` registers (see setup()).
 */
class BluettiHub : public Component {
 public:
  void setup() override;
  void dump_config() override;
  /// Runs before the controllers' setup(), which builds their read ranges from the items registered so far
  float get_setup_priority() const override { return setup_priority::DATA + 1.0f; }

  // ---- Configuration (called by the generated code)
  void set_controller(ModbusController *controller) { this->controller_ = controller; }
  void set_slow_controller(ModbusController *controller) { this->slow_controller_ = controller; }
  void set_model(const char *model) { this->model_ = model; }
  void set_max_span(uint16_t max_span) { this->max_span_ = max_span; }
  void add_block(uint16_t first, uint16_t last) { this->blocks_.emplace_back(first, last); }

  /// Register an entity with the controller that polls it (`slow`: the slower one, when there is one)
  void add_item(SensorItem *item, bool slow);

  /// Queue a "write single register" (FC 0x06). The device confirms it with its own internal address in the echo
  /// (57001 -> 2011, ...); the ESPHome Modbus client does not compare that address with the request.
  void write_register(uint16_t reg, uint16_t value);

#ifdef USE_BINARY_SENSOR
  void set_connection_sensor(binary_sensor::BinarySensor *sensor) { this->connection_sensor_ = sensor; }
#endif

 protected:
  /// Index of the register block that holds [start, start + count), or -1
  int block_of_(uint16_t start, uint16_t count) const;
  /// Cut the read ranges at the block boundaries and every max_span registers, then hand the items over
  void register_items_(std::vector<SensorItem *> &items, ModbusController *controller);
  void set_connected_(bool connected);
  BluettiWriter &writer_for_(uint16_t reg);

  ModbusController *controller_{nullptr};
  ModbusController *slow_controller_{nullptr};
  const char *model_{""};
  uint16_t max_span_{50};
  std::vector<std::pair<uint16_t, uint16_t>> blocks_;
  std::vector<SensorItem *> fast_items_;
  std::vector<SensorItem *> slow_items_;
  uint16_t fast_ranges_{0};
  uint16_t slow_ranges_{0};

  BluettiWriter switch_writer_;
  BluettiWriter soc_writer_;
  BluettiWriter mode_writer_;

  BluettiDepReader heartbeat_reader_;  // first register of the device: tells that it answers (connection sensor)

  bool connected_{false};
  bool connected_known_{false};
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *connection_sensor_{nullptr};
#endif
};

}  // namespace esphome::bluetti
