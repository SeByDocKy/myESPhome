#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/modbus_controller/modbus_controller.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace esphome::deye_mono {

using modbus::EntityType;
using modbus::helpers::SensorValueType;
using modbus_controller::ModbusController;
using modbus_controller::RangeReuse;
using modbus_controller::SensorItem;
using modbus_controller::WriterEntity;

class DeyeMonoHub;

/// raw register -> value:  (raw + add) * scale.  `wrap` first maps the 16-bit values above 32767 to raw - 65535
/// (the start times of the time-of-use slots are stored that way). The hub's inverter factor and the sign are
/// already folded into `scale` by the code generator.
struct Transform {
  float add{0.0f};
  float scale{1.0f};
  bool wrap{false};

  double apply(int64_t raw) const {
    double value = static_cast<double>(raw);
    if (this->wrap && raw > 32767)
      value -= 65535.0;
    return (value + static_cast<double>(this->add)) * static_cast<double>(this->scale);
  }
};

/** Base of every entity that reads a holding register.
 *
 *  The controller resolves where the item sits in the read range it builds (`offset`, a byte offset into the
 *  response); the register the entity was declared with is kept apart in `reg_` (it is the one used for writes).
 *  `parse_and_publish()` is called with the whole range payload; the entity reads its value at `offset`.
 */
class DeyeMonoItem : public SensorItem {
 public:
  void configure(uint16_t reg, SensorValueType vtype) {
    this->reg_ = reg;
    this->register_type = EntityType::HOLDING;
    this->set_address(reg);
    this->set_offset_from_start_address(0);
    this->sensor_value_type = vtype;
    this->bitmask = 0xFFFFFFFF;
    this->count_ = static_cast<uint8_t>(modbus::helpers::register_width_for(vtype));
  }
  uint16_t reg() const { return this->reg_; }

  /// Join the read range before this register even across a gap of unused registers (one frame instead of two)
  void set_bridge(bool bridge) { this->reuse_previous_range = bridge ? RangeReuse::ALWAYS : RangeReuse::AUTO; }

 protected:
  /// True when the payload holds all the registers of this item
  bool has_data_(std::span<const uint8_t> data) const {
    return data.size() >= static_cast<size_t>(this->offset) + static_cast<size_t>(this->count_) * 2u;
  }
  /// Numeric value (16/32 bit, signed or not, according to `sensor_value_type`)
  int64_t raw_(std::span<const uint8_t> data) const {
    return modbus::helpers::payload_to_number(data, this->sensor_value_type, this->offset, this->bitmask).value_or(0);
  }
  /// The first register of this item (host order)
  uint16_t word_(std::span<const uint8_t> data) const {
    const size_t pos = static_cast<size_t>(this->offset);
    return static_cast<uint16_t>((static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]);
  }

  uint16_t reg_{0};
  uint8_t count_{1};
};

/** Reads a register on behalf of a calculated sensor, so that it does not require the source sensors to be declared
 *  in the YAML. Registers next to the declared entities are still merged into the same read range. */
class DeyeMonoDepReader : public DeyeMonoItem {
 public:
  void set_transform(const Transform &transform) { this->transform_ = transform; }
  void set_callback(std::function<void()> &&callback) { this->callback_ = std::move(callback); }
  bool valid() const { return this->valid_; }
  double value() const { return this->value_; }
  void parse_and_publish(std::span<const uint8_t> data) override {
    if (!this->has_data_(data))
      return;
    this->value_ = this->transform_.apply(this->raw_(data));
    this->valid_ = true;
    if (this->callback_)
      this->callback_();
  }

 protected:
  Transform transform_{};
  double value_{0.0};
  bool valid_{false};
  std::function<void()> callback_{};
};

/// Write access to the inverter through the ESPHome Modbus controller
class DeyeMonoWriter : public WriterEntity {
 public:
  void init(ModbusController *controller) { this->set_controller_(controller); }
  using WriterEntity::write_multiple_registers;
  using WriterEntity::write_single_register;
};

/** Deye single-phase hybrid inverter, Modbus.
 *
 *  Sits on top of the ESPHome `modbus` bus and `modbus_controller` (declared in the YAML): the controller builds
 *  the read ranges from the entities registered with add_item(), sends them and handles the retries and the offline
 *  detection. The hub itself only holds what the entities share:
 *   - a copy of the registers that carry several switches / selects, as last read (or written), so that a write
 *     changes only the bits of the entity and keeps the others;
 *   - the hold-off after a write.
 *  The hub does not know the platforms (it includes none of their headers): they register themselves.
 */
class DeyeMonoHub : public Component {
 public:
  void setup() override;
  void dump_config() override;
  /// Runs before the controller's setup(), which builds its read ranges from the items registered so far
  float get_setup_priority() const override { return setup_priority::DATA + 1.0f; }

  // ---- Configuration (called by the generated code)
  void set_controller(ModbusController *controller) { this->controller_ = controller; }
  void set_inverter_factor(float factor) { this->inverter_factor_ = factor; }
  void set_use_write_multiple(bool use) { this->use_write_multiple_ = use; }
  void set_write_hold(uint32_t ms) { this->write_hold_ms_ = ms; }

  /// Register an entity with the controller that polls it
  void add_item(SensorItem *item);
  /// A writable entity declares the register it writes: the hub keeps a copy of it and the hold-off state
  void track_register(uint16_t reg);

  // ---- Used by the writable entities
  /** Called with every value read from a tracked register. Returns false while the register is held after a write
   *  (the value is then stale and must be ignored); otherwise the copy is updated and true is returned. */
  bool accept_read(uint16_t reg, uint16_t value);
  /// Write a whole register
  bool write_register(uint16_t reg, uint16_t value);
  /// Write only the bits of `mask` of a register: the other bits keep the value last read. `bits` is already in
  /// position (not shifted). Refused when the register has not been read yet.
  bool write_masked(uint16_t reg, uint16_t mask, uint16_t bits);

 protected:
  struct RegisterState {
    uint16_t reg;
    uint16_t value;
    uint32_t hold_until;
    bool valid;
    bool hold;
  };
  RegisterState *find_(uint16_t reg);

  ModbusController *controller_{nullptr};
  DeyeMonoWriter writer_;
  float inverter_factor_{1.0f};
  bool use_write_multiple_{true};
  uint32_t write_hold_ms_{5000};
  std::vector<RegisterState> registers_;
};

}  // namespace esphome::deye_mono
