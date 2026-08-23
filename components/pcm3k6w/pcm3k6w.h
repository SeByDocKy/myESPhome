#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/canbus/canbus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"

#include <string>
#include <vector>
#include <cstdint>

namespace esphome::pcm3k6w {

// ---------------------------------------------------------------------------
// Entity "kind" enumerations.
//
// These are the contract between the C++ hub and the Python codegen: each
// platform's __init__.py defines plain int KIND_* constants in this exact
// order (see the comment at the top of each platform __init__.py). Do not
// reorder / insert without updating both sides.
// ---------------------------------------------------------------------------

enum SensorKind : uint8_t {
  SENS_GRID_VOLTAGE = 0,
  SENS_INVERTER_VOLTAGE,
  SENS_BUS_VOLTAGE,
  SENS_INVERTING_CURRENT,
  SENS_DC_VOLTAGE,
  SENS_DC_CURRENT,
  SENS_LOAD_CURRENT,
  SENS_AC_POWER,
  SENS_DC_POWER,
  SENS_TEMPERATURE_PFC,
  SENS_TEMPERATURE_LLC,
  SENS_RUNNING_MODE_VALUE,
  SENS_OFFGRID_FREQUENCY,
  SENS_FREQUENCY_SETTING_READBACK,
  SENS_AC_VOLTAGE_SETTING_READBACK,
  SENS_PHASE_MODE_READBACK,
  SENS_CHARGING_VOLTAGE_READBACK,
  SENS_CHARGING_CURRENT_READBACK,
  SENS_DISCHARGING_CURRENT_READBACK,
  SENS_AC_OVERVOLTAGE_PROTECTION_READBACK,
  SENS_AC_UNDERVOLTAGE_PROTECTION_READBACK,
  SENS_AC_OVERVOLTAGE_ALARM_READBACK,
  SENS_AC_UNDERVOLTAGE_ALARM_READBACK,
  SENS_DC_OVERVOLTAGE_PROTECTION_READBACK,
  SENS_DC_UNDERVOLTAGE_PROTECTION_READBACK,
  SENS_DC_OVERVOLTAGE_ALARM_READBACK,
  SENS_DC_UNDERVOLTAGE_ALARM_READBACK,
  SENS_FAN_SPEED_READBACK,
  SENS_GRID_MODE_READBACK,
  SENS_COUNT
};

enum BinarySensorKind : uint8_t {
  BSENS_BATTERY_UNDERVOLTAGE_ALARM = 0,
  BSENS_BATTERY_OVERVOLTAGE_ALARM,
  BSENS_GRID_UNDERVOLTAGE_ALARM,
  BSENS_GRID_OVERVOLTAGE_ALARM,
  BSENS_FAULT_STATUS,
  BSENS_FAULT_SOFT_START_TIMEOUT,
  BSENS_FAULT_BUS_OVERVOLTAGE,
  BSENS_FAULT_BUS_UNDERVOLTAGE,
  BSENS_FAULT_INVERTER_OVERVOLTAGE,
  BSENS_FAULT_INVERTER_UNDERVOLTAGE,
  BSENS_FAULT_INVERTER_RMS_OVERCURRENT,
  BSENS_FAULT_INVERTER_FAST_OVERCURRENT,
  BSENS_FAULT_DC_OVERVOLTAGE,
  BSENS_FAULT_DC_UNDERVOLTAGE,
  BSENS_FAULT_DC_FAST_OVERCURRENT,
  BSENS_FAULT_DC_RMS_OVERCURRENT,
  BSENS_FAULT_LOAD_OVERCURRENT,
  BSENS_FAULT_SOFT_START_SHORT_CIRCUIT,
  BSENS_FAULT_OUTPUT_OVERLOAD,
  BSENS_FAULT_OVER_TEMPERATURE,
  BSENS_FAULT_FAN_FAULT,
  BSENS_FAULT_SYNC_SIGNAL_LOSS,
  BSENS_FAULT_LOCKED,
  BSENS_FAULT_ADDRESS_FAULT,
  BSENS_FAULT_SN_REPETITION,
  BSENS_FAULT_LOAD_RMS_OVERCURRENT,
  BSENS_FAULT_OVERLOAD,
  BSENS_COUNT
};

enum SwitchKind : uint8_t {
  SW_POWER = 0,
  SW_DISCHARGE_CHARGE,
  SW_MANUAL_FAN_CONTROL,
  SW_COUNT
};

enum NumberKind : uint8_t {
  NUM_CHARGING_VOLTAGE = 0,
  NUM_CHARGING_CURRENT,
  NUM_DISCHARGING_CURRENT,
  NUM_AC_OVERVOLTAGE_PROTECTION,
  NUM_AC_UNDERVOLTAGE_PROTECTION,
  NUM_AC_OVERVOLTAGE_ALARM,
  NUM_AC_UNDERVOLTAGE_ALARM,
  NUM_DC_OVERVOLTAGE_PROTECTION,
  NUM_DC_UNDERVOLTAGE_PROTECTION,
  NUM_DC_OVERVOLTAGE_ALARM,
  NUM_DC_UNDERVOLTAGE_ALARM,
  NUM_FAN_SPEED,
  NUM_COUNT
};

enum SelectKind : uint8_t {
  SEL_GRID_MODE = 0,
  SEL_PHASE_MODE_SETTING,
  SEL_FREQUENCY_SETTING,
  SEL_AC_VOLTAGE_SETTING,
  SEL_COUNT
};

// ---------------------------------------------------------------------------
// Hub component.
//
// Owns the canbus link and is the single source of truth for every entity's
// state. Entities (see sensor/, binary_sensor/, switch/, number/, select/)
// just hold a pointer back to this hub plus their "kind" and forward
// writes/reads through it. CAN frames are never sent with a blocking
// `delay:` - all writes/queries go through a small TX queue drained in
// loop() with a minimum inter-frame gap, matching the timing the original
// YAML enforced with explicit `delay:` steps.
// ---------------------------------------------------------------------------
class PCM3K6WComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_canbus(canbus::Canbus *canbus) { this->canbus_ = canbus; }
  void set_address(uint8_t address) { this->address_ = address; }
  void set_poll_interval(uint32_t ms) { this->poll_interval_ = ms; }
  void set_frame_gap(uint32_t ms) { this->frame_gap_ = ms; }

  void set_sensor(uint8_t kind, sensor::Sensor *s) {
    if (kind < SENS_COUNT) this->sensors_[kind] = s;
  }
  void set_binary_sensor(uint8_t kind, binary_sensor::BinarySensor *s) {
    if (kind < BSENS_COUNT) this->binary_sensors_[kind] = s;
  }
  void set_switch(uint8_t kind, switch_::Switch *s) {
    if (kind < SW_COUNT) this->switches_[kind] = s;
  }
  void set_number(uint8_t kind, number::Number *n) {
    if (kind < NUM_COUNT) this->numbers_[kind] = n;
  }
  void set_select(uint8_t kind, select::Select *s) {
    if (kind < SEL_COUNT) this->selects_[kind] = s;
  }

  // Called from the entity control()/write_state() overrides.
  void write_switch(uint8_t kind, bool state);
  void write_number(uint8_t kind, float value);
  void write_select(uint8_t kind, const std::string &value);

 protected:
  canbus::Canbus *canbus_{nullptr};
  uint8_t address_{0x01};
  uint32_t poll_interval_{30000};
  uint32_t frame_gap_{100};
  uint32_t last_tx_time_{0};

  struct TxFrame {
    uint32_t can_id;
    std::vector<uint8_t> data;
  };
  std::vector<TxFrame> tx_queue_;

  // cmd_type: 0x02 = write, 0x03 = query. reg is the low command byte
  // (e.g. 0x31 for charging voltage). payload is appended starting at
  // data[2], zero-padded to 8 bytes, matching the PCM's frame layout.
  void enqueue_(uint8_t cmd_type, uint8_t reg, const std::vector<uint8_t> &payload);
  void enqueue_query_(uint8_t reg) { this->enqueue_(0x03, reg, {}); }

  void on_frame_(uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data);
  void poll_();

  sensor::Sensor *sensors_[SENS_COUNT] = {nullptr};
  binary_sensor::BinarySensor *binary_sensors_[BSENS_COUNT] = {nullptr};
  switch_::Switch *switches_[SW_COUNT] = {nullptr};
  number::Number *numbers_[NUM_COUNT] = {nullptr};
  select::Select *selects_[SEL_COUNT] = {nullptr};

  void publish_sensor_(uint8_t kind, float value) {
    if (this->sensors_[kind] != nullptr) this->sensors_[kind]->publish_state(value);
  }
  void publish_binary_sensor_(uint8_t kind, bool value) {
    if (this->binary_sensors_[kind] != nullptr) this->binary_sensors_[kind]->publish_state(value);
  }

  // Current value of a number, falling back to `fallback` if the entity
  // wasn't configured in YAML (needed because several registers pack two
  // logical values - e.g. AC over/undervoltage - into one CAN frame, so
  // writing one number must resend the other's last known value too).
  float number_state_or_(uint8_t kind, float fallback) const {
    return (this->numbers_[kind] != nullptr) ? this->numbers_[kind]->state : fallback;
  }
};

}  // namespace esphome::pcm3k6w
