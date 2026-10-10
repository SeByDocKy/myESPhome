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
#include <vector>

namespace esphome::anker_solix {

using modbus::EntityType;
using modbus::helpers::SensorValueType;
using modbus_controller::ModbusController;
using modbus_controller::RangeReuse;
using modbus_controller::SensorItem;
using modbus_controller::WriterEntity;

// Register map of the Solarbank family (input registers are read with FC04, holding registers with FC03).
static constexpr uint16_t REG_MAX_CHARGE_POWER = 10036;      // input, INT32, W
static constexpr uint16_t REG_MAX_DISCHARGE_POWER = 10038;   // input, INT32, W
static constexpr uint16_t REG_OPERATING_MODE = 10064;        // holding, UINT16
static constexpr uint16_t REG_POWER_SETPOINT = 10071;        // holding, INT32, W: charge < 0, discharge > 0
static constexpr uint16_t REG_CHARGING_LIMIT_SOC = 60000;    // holding, UINT16, %
static constexpr uint16_t REG_DISCHARGE_LIMIT_SOC = 60001;   // holding, UINT16, %
static constexpr uint16_t REG_BACKUP_RESERVE_SOC = 60002;    // holding, UINT16, %
static constexpr uint16_t REG_BACKUP_SOC_ENABLE = 60003;     // holding, UINT16, 0 / 1
static constexpr uint16_t REG_EMS_MODE_MASK = 32774;         // input, 0x8006: operating modes the device supports
static constexpr uint16_t REG_PARALLEL_CAPABILITY_MASK = 32775;  // input, 0x8007: SOC limit functions it supports

// Value of register 10064 that hands the battery over to the commands written in register 10071
static constexpr uint16_t MODE_THIRD_PARTY_CONTROL = 3;

class AnkerSolixHub;

/** Base of every entity that reads a register.
 *
 *  The controller resolves where the item sits in the read range it builds (`offset`, a byte offset into the
 *  response); the register the entity was declared with is kept apart in `reg_` (it is the one used for writes).
 *  `parse_and_publish()` is called with the whole range payload; the entity reads its value at `offset`.
 */
class AnkerSolixRegisterItem : public SensorItem {
 public:
  void configure(uint16_t reg, uint8_t count, SensorValueType vtype, EntityType rtype) {
    this->reg_ = reg;
    this->register_type = rtype;
    this->set_address(reg);
    this->set_offset_from_start_address(0);
    this->sensor_value_type = vtype;
    this->bitmask = 0xFFFFFFFF;
    // A string (RAW) spans `count` registers
    if (vtype == SensorValueType::RAW)
      this->set_register_size(static_cast<uint8_t>(count * 2u));
    this->count_ = count;
  }
  uint16_t reg() const { return this->reg_; }

  /// Never merge this register into a read range with its neighbours (it is read on its own)
  void set_own_range(bool own) { this->reuse_previous_range = own ? RangeReuse::NEVER : RangeReuse::AUTO; }

  /// Write protection: the values read back are ignored for `ms` after a write, so that the entity does not jump
  /// back to its old value while the battery is still applying the command.
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
  /// Numeric value (16/32 bit, signed or not, high register first, according to `sensor_value_type`)
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

/** Reads a register on behalf of the hub or of a calculated entity (pv_power), so that those do not require the
 *  source sensors to be declared in the YAML. Neighbouring registers are still merged into the same read range as
 *  the declared entities. */
class AnkerSolixDepReader : public AnkerSolixRegisterItem {
 public:
  void set_callback(std::function<void(int64_t)> &&callback) { this->callback_ = std::move(callback); }
  bool valid() const { return this->valid_; }
  int64_t value() const { return this->value_; }
  void parse_and_publish(std::span<const uint8_t> data) override {
    if (!this->has_data_(data) || this->held_())
      return;
    this->value_ = this->raw_(data);
    this->valid_ = true;
    if (this->callback_)
      this->callback_(this->value_);
  }

 protected:
  int64_t value_{0};
  bool valid_{false};
  std::function<void(int64_t)> callback_{};
};

/// Write access to the battery through the ESPHome Modbus controller (one instance per kind of write, so that the
/// requests of one kind never share a queue slot with another).
class AnkerSolixWriter : public WriterEntity {
 public:
  void init(ModbusController *controller) { this->set_controller_(controller); }
  using WriterEntity::write_multiple_registers;
  using WriterEntity::write_single_register;
};

/** Anker SOLIX Solarbank battery (Max AC, Max, XE, XE AC, 4 E5000 Pro), Modbus.
 *
 *  Sits on top of the ESPHome `modbus` bus and `modbus_controller` (declared in the YAML), whatever the transport under the bus
 *  (a UART on RS485, or serial-over-TCP): the controller builds
 *  the read ranges, sends them and handles the retries and the offline detection. The entities (sensor, number,
 *  ...) register themselves with add_item(); their registers, types and scales come from the register table (see
 *  registers.py). The slow entities (energy totals, versions, SOC limits...) go to a second controller with a
 *  slower `update_interval` when one is given (`slow_modbus_controller_id`).
 *
 *  The hub also holds what a battery that is controlled from the outside needs to stay safe:
 *   - the power setpoint (register 10071, charge < 0, discharge > 0) is limited to what the device reports it can
 *     do (registers 10036 / 10038) and to the optional max_charge_power / max_discharge_power of the YAML;
 *   - the setpoint is only taken into account in the "Third-Party Controlled" mode: with auto_third_party_control
 *     the hub selects that mode itself (and again if the Anker app changed it);
 *   - the state of charge limits follow the rules of the official integration (see check_soc_write()).
 */
class AnkerSolixHub : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  /// Runs before the controllers' setup(), which builds their read ranges from the items registered so far
  float get_setup_priority() const override { return setup_priority::DATA + 1.0f; }

  // ---- Configuration (called by the generated code)
  void set_controller(ModbusController *controller) { this->controller_ = controller; }
  void set_slow_controller(ModbusController *controller) { this->slow_controller_ = controller; }
  void set_model(const char *model) { this->model_ = model; }
  void set_auto_third_party_control(bool enable) { this->auto_third_party_control_ = enable; }
  void set_max_charge_power(uint32_t watts) { this->max_charge_override_w_ = watts; }
  void set_max_discharge_power(uint32_t watts) { this->max_discharge_override_w_ = watts; }
  /// Re-send the last power setpoint when nothing was written for this long (0 = never)
  void set_setpoint_refresh(uint32_t ms) { this->setpoint_refresh_ms_ = ms; }
  /// Read the capability mask (register 0x8007) that tells which SOC limit functions the device supports
  void set_read_capability_mask(bool read) { this->read_capability_mask_ = read; }
  /// The platforms ask for what they need to read in the background
  void enable_mode_tracking() { this->track_mode_ = true; }
  void enable_power_limits() { this->power_limits_ = true; }
  void enable_soc_rules() { this->soc_rules_ = true; }

  /// Register an entity with the controller that polls it (`slow`: the slower one, when there is one)
  void add_item(SensorItem *item, bool slow);

#ifdef USE_BINARY_SENSOR
  void set_connection_sensor(binary_sensor::BinarySensor *sensor) { this->connection_sensor_ = sensor; }
#endif

  // ---- Commands
  /// Queue a "write single register" (FC 0x06)
  void write_register(uint16_t reg, uint16_t value);
  /// Power setpoint in W: charge (grid -> battery) < 0, discharge (AC output) > 0. Written as an INT32 (FC 0x10).
  void write_power_setpoint(int32_t watts);
  /// Fraction 0..1 of the maximum charge (`charge` true) or discharge power; the request is discharge - charge.
  void set_power_fraction(bool charge, float fraction);
  /// Select an operating mode. `capability_bit` is the bit of the EMS mode mask that says the device supports it.
  /// Returns false (and writes nothing) when the mask says it does not.
  bool request_operating_mode(uint16_t mode, uint8_t capability_bit);
  /// Check a write of a state of charge limit (60000..60002); returns nullptr when it is allowed, else the reason.
  const char *check_soc_write(uint16_t reg, uint16_t value, uint8_t capability_bit, bool needs_backup_enable);
  /// Remember what was written, so the next checks use it before the value is read back
  void note_soc_written(uint16_t reg, uint16_t value);

  int32_t power_limit_charge() const { return this->effective_max_(this->max_charge_w_, this->max_charge_override_w_); }
  int32_t power_limit_discharge() const {
    return this->effective_max_(this->max_discharge_w_, this->max_discharge_override_w_);
  }

 protected:
  static int32_t effective_max_(int32_t device, uint32_t override_w) {
    int32_t limit = device > 0 ? device : 0;
    if (override_w > 0 && (limit == 0 || static_cast<int32_t>(override_w) < limit))
      limit = static_cast<int32_t>(override_w);
    return limit;  // 0 = unknown
  }
  void send_power_setpoint_(int32_t watts);
  void ensure_third_party_control_();
  void set_connected_(bool connected);
  AnkerSolixWriter &writer_for_(uint16_t reg);

  ModbusController *controller_{nullptr};
  ModbusController *slow_controller_{nullptr};
  const char *model_{""};
  bool auto_third_party_control_{false};
  uint32_t max_charge_override_w_{0};
  uint32_t max_discharge_override_w_{0};
  uint32_t setpoint_refresh_ms_{0};
  bool read_capability_mask_{true};

  // What the platforms asked the hub to read in the background
  bool track_mode_{false};
  bool power_limits_{false};
  bool soc_rules_{false};
  AnkerSolixDepReader mode_reader_;
  AnkerSolixDepReader ems_mask_reader_;
  AnkerSolixDepReader max_charge_reader_;
  AnkerSolixDepReader max_discharge_reader_;
  AnkerSolixDepReader soc_readers_[3];
  AnkerSolixDepReader backup_reader_;
  AnkerSolixDepReader capability_reader_;
  AnkerSolixDepReader heartbeat_reader_;  // battery status: tells that the battery answers (connection sensor)

  AnkerSolixWriter mode_writer_;
  AnkerSolixWriter setpoint_writer_;
  AnkerSolixWriter soc_writer_;
  AnkerSolixWriter other_writer_;

  // Last values read or written (-1 = not known yet)
  int32_t mode_{-1};
  int32_t ems_mask_{-1};
  int32_t capability_mask_{-1};
  int32_t max_charge_w_{0};
  int32_t max_discharge_w_{0};
  int32_t soc_[3]{-1, -1, -1};  // charging limit, discharge limit, backup reserve
  int32_t backup_enable_{-1};

  float charge_fraction_{0.0f};
  float discharge_fraction_{0.0f};
  int32_t setpoint_w_{0};
  bool setpoint_valid_{false};
  uint32_t last_setpoint_ms_{0};
  uint32_t last_mode_write_ms_{0};
  bool mode_write_sent_{false};
  uint32_t last_limit_warning_ms_{0};

  bool connected_{false};
  bool connected_known_{false};
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *connection_sensor_{nullptr};
#endif
};

}  // namespace esphome::anker_solix
