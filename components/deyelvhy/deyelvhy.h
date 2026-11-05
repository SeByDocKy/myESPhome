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

namespace esphome::deyelvhy {

using modbus::EntityType;
using modbus::helpers::SensorValueType;
using modbus_controller::ModbusController;
using modbus_controller::RangeReuse;
using modbus_controller::SensorItem;
using modbus_controller::WriterEntity;

class DeyeLvHyHub;

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
class DeyeLvHyItem : public SensorItem {
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

  /// This register may be joined to the read range before it, even across a gap of unused registers (one frame
  /// instead of two). The hub decides when it registers the item (see DeyeLvHyHub::setup()).
  void set_bridge(bool bridge) { this->bridge_ = bridge; }
  bool bridge() const { return this->bridge_; }
  uint8_t count() const { return this->count_; }
  /// The value of this register is in units of the inverter's power ratio (register 54): the hub holds the ratio
  void set_ratio(DeyeLvHyHub *hub) { this->ratio_hub_ = hub; }
  void apply_bridge(bool join) { this->reuse_previous_range = join ? RangeReuse::ALWAYS : RangeReuse::AUTO; }

 protected:
  /// False while the power ratio of the inverter is not known yet (the value cannot be converted)
  bool ratio_ready_() const;
  /// Multiplier of the converted value: the inverter's power ratio, or 1 for a register that does not depend on it
  double ratio_() const;
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
  bool bridge_{false};
  DeyeLvHyHub *ratio_hub_{nullptr};
};

/** Reads a register on behalf of a calculated sensor, so that it does not require the source sensors to be declared
 *  in the YAML. Registers next to the declared entities are still merged into the same read range. */
class DeyeLvHyDepReader : public DeyeLvHyItem {
 public:
  void set_transform(const Transform &transform) { this->transform_ = transform; }
  void set_callback(std::function<void()> &&callback) { this->callback_ = std::move(callback); }
  bool valid() const { return this->valid_; }
  double value() const { return this->value_; }
  void parse_and_publish(std::span<const uint8_t> data) override {
    if (!this->has_data_(data))
      return;
    if (!this->ratio_ready_())
      return;
    this->value_ = this->transform_.apply(this->raw_(data)) * this->ratio_();
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
class DeyeLvHyWriter : public WriterEntity {
 public:
  void init(ModbusController *controller) { this->set_controller_(controller); }
  using WriterEntity::write_multiple_registers;
  using WriterEntity::write_single_register;
};

/** Deye low-voltage hybrid inverter (single-phase or three-phase), Modbus.
 *
 *  Sits on top of the ESPHome `modbus` bus and `modbus_controller` (declared in the YAML): the controller builds
 *  the read ranges from the entities registered with add_item(), sends them and handles the retries and the offline
 *  detection. The hub itself only holds what the entities share:
 *   - a copy of the registers that carry several switches / selects, as last read (or written), so that a write
 *     changes only the bits of the entity and keeps the others;
 *   - the hold-off after a write.
 *  Two controllers can poll the same inverter at two speeds: the entities of the `deyelvhy_fast` platforms go to the
 *  fast one, those of `deyelvhy_slow` to the slow one (the fast one when there is no slow one). Every write goes
 *  through the fast controller.
 *  The platforms register themselves; their classes live next to the hub and are compiled only when the matching
 *  ESPHome component is used (USE_SENSOR...), because the `deyelvhy_fast` / `deyelvhy_slow` platform directories share
 *  one code base.
 */
class DeyeLvHyHub : public Component {
 public:
  void setup() override;
  void dump_config() override;
  /// Runs before the controller's setup(), which builds its read ranges from the items registered so far
  float get_setup_priority() const override { return setup_priority::DATA + 1.0f; }

  // ---- Configuration (called by the generated code)
  /// The controller that polls the fast entities, and also sends every write
  void set_fast_controller(ModbusController *controller) { this->fast_controller_ = controller; }
  /// Optional second controller (same inverter, slower update_interval) for the slow entities
  void set_slow_controller(ModbusController *controller) { this->slow_controller_ = controller; }
  void set_phase(const char *phase) { this->phase_ = phase; }
  /// A fixed multiplier for the registers that depend on the power ratio (instead of reading register 54)
  void set_inverter_factor(float factor) {
    this->inverter_factor_ = factor;
    this->ratio_ = factor;
    this->auto_factor_ = false;
  }
  /// Read the power ratio from register 54 (the default)
  void set_inverter_factor_auto() { this->auto_factor_ = true; }
  /// An entity depends on the power ratio: in `auto` mode the hub starts reading register 54
  void need_ratio();
  bool ratio_known() const { return this->ratio_known_; }
  float ratio() const { return this->ratio_; }
  void set_use_write_multiple(bool use) { this->use_write_multiple_ = use; }
  void set_write_hold(uint32_t ms) { this->write_hold_ms_ = ms; }

  /// Declare an entity (`slow`: polled by the slow controller, when there is one). The controllers get them in setup().
  void add_item(DeyeLvHyItem *item, bool slow);
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
  void register_items_(bool slow);

  struct PendingItem {
    DeyeLvHyItem *item;
    bool slow;
  };
  std::vector<PendingItem> items_;  // the entities, until setup() hands them to the controllers

  ModbusController *fast_controller_{nullptr};
  ModbusController *slow_controller_{nullptr};
  const char *phase_{""};
  DeyeLvHyWriter writer_;
  float inverter_factor_{1.0f};
  bool auto_factor_{true};
  bool ratio_needed_{false};
  bool ratio_known_{false};
  float ratio_{1.0f};
  DeyeLvHyDepReader ratio_reader_;
  bool use_write_multiple_{true};
  uint32_t write_hold_ms_{5000};
  std::vector<RegisterState> registers_;
};

}  // namespace esphome::deyelvhy
