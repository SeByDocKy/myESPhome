#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/modbus_controller/modbus_controller.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <cmath>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace esphome::ecoflow_stream {

using modbus::EntityType;
using modbus::helpers::SensorValueType;
using modbus_controller::ModbusController;
using modbus_controller::SensorItem;
using modbus_controller::WriterEntity;

class EcoflowStreamHub;

// Modbus addresses on the wire (offset of the EcoFlow point table + 40001)
static constexpr uint16_t REG_HEARTBEAT = 40608;
static constexpr uint16_t REG_MAX_OUTPUT_POWER = 40546;  // UINT32, W (discharge side)
static constexpr uint16_t REG_MAX_INPUT_POWER = 40548;   // UINT32, W (charge side)
static constexpr uint16_t REG_MASTER_FLAG = 44098;
static constexpr uint16_t REG_CONTROL_POINT = 45649;
static constexpr uint16_t REG_TARGET_BASE = 45649;  // + control point: the INT16 target register of that point

/// Values of the control point register that the charge / discharge outputs can use
enum class ControlPoint : uint8_t { BATTERY = 1, INVERTER = 3, GRID_PORT = 4 };

// After a write the device needs a moment to apply the command: the values read back meanwhile may be the old ones
static constexpr uint32_t WRITE_HOLD_MS = 10000;

/// Write access to the device through the ESPHome Modbus controller (one instance per writer, so that the requests
/// of two writers never share a queue slot).
class EcoflowStreamWriter : public WriterEntity {
 public:
  void init(ModbusController *controller) { this->set_controller_(controller); }
  using WriterEntity::write_multiple_registers;
  using WriterEntity::write_single_register;
};

/** Base of every entity that reads a register.
 *
 *  The controller resolves where the item sits in the read range it builds (`offset`, a byte offset into the
 *  response). STREAM registers are holding registers (function code 03); 32 bit values come high word first.
 */
class EcoflowStreamRegisterItem : public SensorItem {
 public:
  void configure(uint16_t reg, uint8_t count, SensorValueType vtype, EntityType rtype) {
    this->reg_ = reg;
    this->register_type = rtype;
    this->set_address(reg);
    this->set_offset_from_start_address(0);
    this->sensor_value_type = vtype;
    this->bitmask = 0xFFFFFFFF;
    // Strings and code lists (RAW) span `count` registers
    if (vtype == SensorValueType::RAW)
      this->set_register_size(static_cast<uint8_t>(count * 2u));
    this->count_ = count;
  }
  uint16_t reg() const { return this->reg_; }
  uint8_t count() const { return this->count_; }
  /// This item is a value of the whole system: it is only valid on the system master
  void set_system_value(EcoflowStreamHub *hub) { this->system_hub_ = hub; }
  /// Signed battery power: `sign` points to the hub setting giving the sign the device uses while charging.
  /// The entity always shows positive = charge, negative = discharge.
  void set_power_sign(const int8_t *sign) { this->sign_ = sign; }

  /// Write protection: the values read back are ignored for `ms` after a write
  void hold_for(uint32_t ms) {
    this->hold_until_ = millis() + ms;
    this->hold_active_ = true;
  }

 protected:
  bool held_() {
    if (!this->hold_active_)
      return false;
    if (static_cast<int32_t>(millis() - this->hold_until_) >= 0) {
      this->hold_active_ = false;
      return false;
    }
    return true;
  }
  /// -1 when the device counts charging as negative, else 1 (also when the setting is absent)
  int sign_factor_() const { return this->sign_ != nullptr && *this->sign_ < 0 ? -1 : 1; }
  /// True when the payload holds all the registers of this item AND the item may be published by this unit
  bool has_data_(std::span<const uint8_t> data) const;
  int64_t raw_(std::span<const uint8_t> data) const {
    return modbus::helpers::payload_to_number(data, this->sensor_value_type, this->offset, this->bitmask).value_or(0);
  }
  /// Numeric value, IEEE 754 floats included
  float number_(std::span<const uint8_t> data) const {
    const int64_t raw = this->raw_(data);
    if (this->sensor_value_type == SensorValueType::FP32)
      return bit_cast<float>(static_cast<uint32_t>(raw));
    return static_cast<float>(raw);
  }
  uint16_t word_(std::span<const uint8_t> data, size_t index) const {
    const size_t pos = static_cast<size_t>(this->offset) + index * 2u;
    return static_cast<uint16_t>((static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]);
  }

  uint16_t reg_{0};
  uint8_t count_{1};
  uint32_t hold_until_{0};
  bool hold_active_{false};
  EcoflowStreamHub *system_hub_{nullptr};
  const int8_t *sign_{nullptr};
};

/// Reads a register for the hub itself, so that it does not require an entity to be declared in the YAML
class EcoflowStreamDepReader : public EcoflowStreamRegisterItem {
 public:
  void set_callback(std::function<void(int64_t)> &&callback) { this->callback_ = std::move(callback); }
  void parse_and_publish(std::span<const uint8_t> data) override {
    if (!this->has_data_(data) || this->held_())
      return;
    if (this->callback_)
      this->callback_(this->raw_(data));
  }

 protected:
  std::function<void(int64_t)> callback_{};
};

/** EcoFlow STREAM 5000 / STREAM AC 5000, Modbus.
 *
 *  Sits on top of the ESPHome `modbus` bus and `modbus_controller` (declared in the YAML): the controller builds the
 *  read ranges (only adjacent registers are merged), sends them and handles the retries and the offline detection.
 *
 *  The hub holds what a battery controlled from the outside needs:
 *   - the heartbeat register, written while a control platform is declared. Without it for 60 s the battery goes
 *     back to its own energy management;
 *   - the system master flag: the system values are only meaningful (and writable) on the master;
 *   - the signed power setpoint of the charge / discharge outputs, limited to what the device reports and to the
 *     optional max_charge_power / max_discharge_power of the YAML.
 */
class EcoflowStreamHub : public Component {
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
  void set_heartbeat_interval(uint32_t ms) { this->heartbeat_interval_ms_ = ms; }
  void set_ignore_master_flag(bool ignore) { this->ignore_master_flag_ = ignore; }
  void set_charge_sign(int8_t sign) { this->charge_sign_ = sign; }
  void set_battery_power_charge_sign(int8_t sign) { this->battery_power_charge_sign_ = sign; }
  const int8_t *target_sign() const { return &this->charge_sign_; }
  const int8_t *measure_sign() const { return &this->battery_power_charge_sign_; }
  void set_control_point(ControlPoint point) { this->control_point_ = point; }
  void set_auto_control_point(bool enable) { this->auto_control_point_ = enable; }
  void set_max_charge_power(uint32_t watts) { this->max_charge_override_w_ = watts; }
  void set_max_discharge_power(uint32_t watts) { this->max_discharge_override_w_ = watts; }
  void set_setpoint_refresh(uint32_t ms) { this->setpoint_refresh_ms_ = ms; }
  /// A platform that writes to the device asks for the heartbeat
  void enable_control() { this->control_enabled_ = true; }
  /// The output platform asks for what it needs to read in the background
  void enable_outputs() {
    this->control_enabled_ = true;
    this->outputs_enabled_ = true;
  }

  /// Declare an entity; it is handed to its controller in setup() (`slow`: the slower one, when there is one)
  void add_item(SensorItem *item, bool slow);

#ifdef USE_BINARY_SENSOR
  void set_connection_sensor(binary_sensor::BinarySensor *sensor) { this->connection_sensor_ = sensor; }
#endif

  // ---- State
  ModbusController *controller() const { return this->controller_; }
  /// True when this unit reported itself as the system master (or the flag is ignored)
  bool system_valid() const { return this->ignore_master_flag_ || this->master_ == 1; }

  // ---- Commands
  void set_heartbeat_enabled(bool enabled);
  /// Fraction 0..1 of the maximum charge (`charge` true) or discharge power; the request is discharge - charge
  void set_power_fraction(bool charge, float fraction);

 protected:
  void send_heartbeat_();
  void send_setpoint_();
  void set_connected_(bool connected);
  int32_t effective_max_(int32_t device, uint32_t override_w) const;

  ModbusController *controller_{nullptr};
  ModbusController *slow_controller_{nullptr};
  const char *model_{""};
  uint32_t heartbeat_interval_ms_{20000};
  bool ignore_master_flag_{false};
  int8_t charge_sign_{0};
  int8_t battery_power_charge_sign_{1};
  ControlPoint control_point_{ControlPoint::BATTERY};
  bool auto_control_point_{false};
  uint32_t max_charge_override_w_{0};
  uint32_t max_discharge_override_w_{0};
  uint32_t setpoint_refresh_ms_{0};
  std::vector<SensorItem *> fast_items_;
  std::vector<SensorItem *> slow_items_;

  bool control_enabled_{false};
  bool outputs_enabled_{false};
  bool heartbeat_enabled_{true};
  bool heartbeat_sent_{false};
  uint32_t last_heartbeat_ms_{0};

  EcoflowStreamDepReader master_reader_;  // also tells that the device answers (connection sensor)
  EcoflowStreamDepReader control_point_reader_;
  EcoflowStreamDepReader max_output_reader_;
  EcoflowStreamDepReader max_input_reader_;

  EcoflowStreamWriter heartbeat_writer_;
  EcoflowStreamWriter control_point_writer_;
  EcoflowStreamWriter setpoint_writer_;

  // Last values read (-1 = not known yet)
  int32_t master_{-1};
  int32_t device_control_point_{-1};
  int32_t max_output_w_{0};
  int32_t max_input_w_{0};

  float charge_fraction_{0.0f};
  float discharge_fraction_{0.0f};
  int32_t setpoint_w_{0};  // discharge > 0, charge < 0 (converted to the sign of the device when written)
  bool setpoint_valid_{false};
  uint32_t last_setpoint_ms_{0};

  bool connected_{false};
  bool connected_known_{false};
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *connection_sensor_{nullptr};
#endif
};

inline bool EcoflowStreamRegisterItem::has_data_(std::span<const uint8_t> data) const {
  if (this->system_hub_ != nullptr && !this->system_hub_->system_valid())
    return false;
  return data.size() >= static_cast<size_t>(this->offset) + static_cast<size_t>(this->count_) * 2u;
}

}  // namespace esphome::ecoflow_stream
