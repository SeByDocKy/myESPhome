#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/modbus/modbus.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

#include "sunspec_models.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace esphome::sunspec_client {

// "SunS" marker, two registers: 0x5375 ("Su") then 0x6E53 ("nS")
static constexpr uint16_t SUNSPEC_MARKER_HI = 0x5375;
static constexpr uint16_t SUNSPEC_MARKER_LO = 0x6E53;
// Model id that ends the chain of models
static constexpr uint16_t SUNSPEC_END_MODEL_ID = 0xFFFF;
// Safety cap on the number of models read during a scan
static constexpr uint8_t MAX_SCANNED_MODELS = 64;
// Retries of a request that got no answer, before the scan is declared failed
static constexpr uint8_t MAX_NO_RESPONSE_RETRIES = 3;
// A text sensor state is limited to 255 characters in Home Assistant
static constexpr size_t MAX_TEXT_STATE_LENGTH = 250;
// Registers asked per request (the Modbus limit is 125)
static constexpr uint16_t MAX_REGISTERS_PER_REQUEST = 100;
// Registers of one model kept in memory at most
static constexpr uint16_t MAX_MODEL_REGISTERS = 256;
// Number of PointKind enumerators
static constexpr size_t POINT_KIND_COUNT = 9;

/// One model found during the scan.
struct ModelInfo {
  uint16_t id;       ///< SunSpec model id
  uint16_t length;   ///< number of data registers (header excluded)
  uint16_t address;  ///< register address of the model header
};

enum class ScanState : uint8_t {
  IDLE,            ///< nothing in progress
  PROBING_BASE,    ///< looking for the "SunS" marker at the candidate base addresses
  READING_HEADER,  ///< walking the chain of model headers
  READING_MODEL    ///< reading the registers of a model (first read, then every update_interval)
};

/// A sensor bound to a point of a model.
struct Binding {
  uint8_t model_index;  ///< index in the vector of the wanted and found models
  const PointDef *def;
  uint16_t offset;      ///< registers from the model header
  int16_t sf_offset;    ///< scale factor register from the model header, -1 when none
  uint8_t instance;     ///< instance of the repeating group (0 for the fixed block)
  bool in_group;        ///< point of the repeating group
  uint8_t decimals;     ///< accuracy decimals of the sensor
  int16_t sf_value;     ///< scale factor (power of ten) seen at the last read
  uint8_t control;      ///< 0: read only, 1: number, 2: select (writable point of a model of `controls`)
#ifdef USE_NUMBER
  number::Number *number;
#endif
#ifdef USE_SELECT
  select::Select *select;
#endif
#ifdef USE_SENSOR
  sensor::Sensor *sensor;
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *text_sensor;  ///< set for enum, bitfield and string points
#endif
};

#ifdef USE_BINARY_SENSOR
/// A binary sensor bound to a named bit of a bitfield point.
struct BitBinding {
  uint16_t binding_index;  ///< index in the vector of bindings of the bitfield point
  uint8_t bit;
  binary_sensor::BinarySensor *sensor;
};
#endif

class SunSpecClient;

#ifdef USE_NUMBER
/// Number bound to a writable numeric point: a change writes the register (scale factor applied).
class SunSpecNumber : public number::Number {
 public:
  SunSpecNumber(SunSpecClient *parent, uint16_t binding_index) : parent_(parent), binding_index_(binding_index) {}

 protected:
  void control(float value) override;
  SunSpecClient *parent_;
  uint16_t binding_index_;
};
#endif

#ifdef USE_SELECT
/// Select bound to a writable enum point: the options are the symbols of the enum.
class SunSpecSelect : public select::Select {
 public:
  SunSpecSelect(SunSpecClient *parent, uint16_t binding_index) : parent_(parent), binding_index_(binding_index) {}

 protected:
  void control(size_t index) override;
  SunSpecClient *parent_;
  uint16_t binding_index_;
};
#endif

/// SunSpec client on the core modbus bus.
///
/// The component is itself a modbus client device. After the scan of the model headers, the models of the
/// wish list the device exposes are read, the sensors of the implemented points are created at once and
/// Home Assistant is asked a single time to read the entity list again. All requests go through the hub
/// queue, one at a time, and replies arrive in loop() on the main task.
class SunSpecClient final : public Component, public modbus::ModbusClientDevice {
 public:
  void setup() override;
  void dump_config() override;
  /// Late: the Modbus bus and the link below it (WiFi, TCP) are set up first.
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_startup_delay(uint32_t delay_ms) { this->startup_delay_ms_ = delay_ms; }
  void set_scan_retry_interval(uint32_t interval_ms) { this->scan_retry_interval_ms_ = interval_ms; }
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ms_ = interval_ms; }
  void set_max_modules(uint8_t count) { this->max_modules_ = count; }
  void set_wait_for_network(bool wait) { this->wait_for_network_ = wait; }
  void set_bitfield_binary_sensors(bool enable) { this->bitfield_binary_sensors_ = enable; }
  void set_name_prefix(const std::string &prefix) { this->name_prefix_ = prefix; }
  void add_wanted_model(uint16_t model_id) { this->wanted_models_.push_back(model_id); }
  /// Model whose writable points become number / select controls.
  void add_control_model(uint16_t model_id) { this->control_models_.push_back(model_id); }
  /// Write a value to the first number control built on the point `key` (SunSpec point name, for example
  /// "WMaxLimPct"), as if it were set from Home Assistant. Usable from lambdas. Returns false when there is none.
  bool set_point(const char *key, float value);
  void write_number(uint16_t binding_index, float value);
  void write_select(uint16_t binding_index, size_t index);
  /// Index of a unit string in the generated table of the application (registered by codegen).
  void add_unit_index(const char *unit, uint8_t index) { this->unit_indices_.push_back(UnitIndex{unit, index}); }
  void set_device_class_index(uint8_t kind, uint8_t index) {
    if (kind < POINT_KIND_COUNT)
      this->device_class_indices_[kind] = index;
  }

#ifdef USE_TEXT_SENSOR
  void set_models_text_sensor(text_sensor::TextSensor *sensor) { this->models_text_sensor_ = sensor; }
#endif

  /// Models found by the last successful scan (empty until then).
  const std::vector<ModelInfo> &get_models() const { return this->models_; }
  bool is_scan_done() const { return this->scan_done_; }

  void on_read_holding_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                 modbus::ResponseStatus status) override;
  bool on_no_response(std::span<const uint8_t> request_pdu) override;
  void on_write_single_register(uint16_t address, uint16_t value, modbus::ResponseStatus status) override;

 protected:
  struct UnitIndex {
    const char *unit;
    uint8_t index;
  };

  // Scan
  void start_scan_();
  void probe_base_();
  void read_header_(uint16_t address);
  bool request_(uint16_t address, uint16_t count);
  void finish_scan_();
  void fail_scan_(const char *reason);
  void publish_models_text_(const std::string &text);

  // Models: reading, discovery of the implemented points, entity creation, updates
  void start_cycle_();
  void read_next_model_();
  void request_chunk_();
  void abort_cycle_(const char *reason);
  void on_model_read_();
  void discover_points_(uint8_t model_index);
  void create_entities_();
  void publish_values_(uint8_t model_index);
#ifdef USE_BINARY_SENSOR
  void publish_bits_(uint16_t binding_index, const Binding &b);
#endif
  void schedule_next_cycle_();
  void request_resync_();
  void do_resync_();
  uint8_t unit_index_(const char *unit) const;
  bool is_control_model_(uint16_t id) const;
  bool write_raw_(uint16_t binding_index, uint16_t raw);
  std::string make_name_(const PointDef &def, const ModelDef &model, const char *group_name, uint8_t instance) const;

  uint32_t startup_delay_ms_{5000};
  uint32_t scan_retry_interval_ms_{30000};
  uint32_t update_interval_ms_{10000};
  uint8_t max_modules_{4};
  bool bitfield_binary_sensors_{false};
  bool wait_for_network_{false};
  std::string name_prefix_{};
  std::vector<uint16_t> wanted_models_{};
  std::vector<uint16_t> control_models_{};
  std::vector<UnitIndex> unit_indices_{};
  uint8_t device_class_indices_[POINT_KIND_COUNT]{};

  ScanState state_{ScanState::IDLE};
  bool scan_done_{false};
  uint8_t base_index_{0};
  uint8_t retries_{0};
  uint16_t pending_address_{0};
  std::vector<ModelInfo> models_{};

  // Models to read: indices in models_ of the found models that are also wanted and known
  std::vector<uint8_t> plan_{};
  size_t plan_pos_{0};
  bool entities_created_{false};
  std::vector<uint16_t> buffer_{};  ///< registers of the model being read
  uint16_t buffer_target_{0};       ///< registers to read for the model being read
  std::vector<Binding> bindings_{};
#ifdef USE_BINARY_SENSOR
  std::vector<BitBinding> bit_bindings_{};
#endif
  std::vector<const std::string *> names_{};  ///< names given to the created entities
  uint32_t last_resync_ms_{0};
  bool has_resynced_{false};

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *models_text_sensor_{nullptr};
#endif
};

}  // namespace esphome::sunspec_client
