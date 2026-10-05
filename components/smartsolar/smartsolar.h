#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/vecan/vecan.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace esphome {
namespace smartsolar {

// NOTE: the numeric values are mirrored in the sensor/text_sensor/binary_sensor __init__.py files
// (tests/check_kinds.py verifies that both sides agree).
enum SensorKind : uint8_t {
  SENSOR_BATTERY_VOLTAGE = 0,
  SENSOR_BATTERY_CURRENT = 1,
  SENSOR_BATTERY_POWER = 2,
  SENSOR_BATTERY_TEMPERATURE = 3,
  SENSOR_PV_VOLTAGE = 4,
  SENSOR_PV_CURRENT = 5,
  SENSOR_PV_POWER = 6,
  SENSOR_ENERGY_TODAY = 7,
  SENSOR_ENERGY_YESTERDAY = 8,
  SENSOR_ENERGY_TOTAL = 9,
  SENSOR_MAX_POWER_TODAY = 10,
  SENSOR_MAX_POWER_YESTERDAY = 11,
  SENSOR_INTERNAL_TEMPERATURE = 12,
  SENSOR_ABSORPTION_VOLTAGE = 13,
  SENSOR_FLOAT_VOLTAGE = 14,
  SENSOR_MAX_CHARGE_CURRENT = 15,
  SENSOR_INPUT_VOLTAGE = 16,
  SENSOR_INPUT_POWER = 17,
  SENSOR_OUTPUT_VOLTAGE = 18,
  SENSOR_OUTPUT_CURRENT = 19,
  SENSOR_OUTPUT_POWER = 20,
  SENSOR_CHARGER_MAX_CURRENT = 21,
  SENSOR_BATTERY_TEMPERATURE_REG = 22,
};

enum TextSensorKind : uint8_t {
  TEXT_STATE = 0,
  TEXT_ERROR = 1,
  TEXT_FIRMWARE_VERSION = 2,
  TEXT_MODEL = 3,
  TEXT_SERIAL_NUMBER = 4,
  TEXT_TRACKER_MODE = 5,
  TEXT_ADDITIONAL_STATE = 6,
};

enum BinarySensorKind : uint8_t {
  BINARY_RELAY = 0,
  BINARY_ALARM = 1,
  BINARY_LOW_VOLTAGE = 2,
  BINARY_HIGH_VOLTAGE = 3,
  BINARY_SOLAR_ACTIVITY = 4,
};

enum NumberKind : uint8_t {
  NUMBER_ABSORPTION_VOLTAGE = 0,
  NUMBER_FLOAT_VOLTAGE = 1,
  NUMBER_MAX_CHARGE_CURRENT = 2,
  NUMBER_EQUALIZATION_VOLTAGE = 3,
};

enum SwitchKind : uint8_t {
  SWITCH_CHARGER = 0,  // device mode register: on / off
};

#ifdef USE_SENSOR
class SmartSolarSensor;
#endif
#ifdef USE_TEXT_SENSOR
class SmartSolarTextSensor;
#endif
#ifdef USE_BINARY_SENSOR
class SmartSolarBinarySensor;
#endif
#ifdef USE_NUMBER
class SmartSolarNumber;
#endif
#ifdef USE_SWITCH
class SmartSolarSwitch;
#endif

/// One VE.Can MPPT solar charger (BlueSolar / SmartSolar MPPT 150/xx with VE.Can port).
///
/// Live values (battery / PV voltage and current, relay and alarm bits) are broadcast by the charger as standard
/// NMEA 2000 PGNs and are simply listened to. Everything else is read from Victron registers (VREGs), which are
/// requested every `poll_interval` (static ones - firmware, model, serial - only until they have been received).
class SmartSolar : public PollingComponent, public vecan::VeCanDevice {
 public:
  void set_vecan(vecan::VeCanHub *hub) { this->parent_ = hub; }
  void set_battery_instance(uint8_t instance) { this->battery_instance_ = instance; }
  void set_pv_instance(uint8_t instance) { this->pv_instance_ = instance; }

#ifdef USE_SENSOR
  void register_sensor(SmartSolarSensor *sensor);
#endif
#ifdef USE_TEXT_SENSOR
  void register_text_sensor(SmartSolarTextSensor *sensor);
#endif
#ifdef USE_BINARY_SENSOR
  void register_binary_sensor(SmartSolarBinarySensor *sensor);
#endif

#ifdef USE_NUMBER
  void register_number(SmartSolarNumber *number);
  /// Request a new value (V or A). The entity only changes once the charger has confirmed it.
  void write_number(uint8_t kind, float value);
#endif
#ifdef USE_SWITCH
  void register_switch(SmartSolarSwitch *sw);
  void write_switch(uint8_t kind, bool state);
#endif

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA - 1.0f; }

  // vecan::VeCanDevice
  void on_pgn(uint32_t pgn, const uint8_t *data, uint8_t len) override;
  void on_vreg(uint16_t reg, const uint8_t *data, uint16_t len) override;
  void on_vreg_nack(uint16_t reg, uint16_t code) override;

 protected:
  void publish_sensor_(uint8_t kind, float value);
  void publish_text_sensor_(uint8_t kind, const std::string &value);
  void publish_binary_sensor_(uint8_t kind, bool value);
  void request_(uint16_t reg);

  // --- register writes -------------------------------------------------------------------------
  // Safety rules, because these registers live in the charger's non-volatile memory:
  //  * a value equal to the one the charger already reports is never written
  //  * writes are debounced (a slider being dragged produces one write) and spaced per register
  //  * the value is hard-limited, and float <= absorption is enforced when both are known
  //  * the charger must confirm (broadcast of the new value); otherwise the register is read back, and the
  //    entity returns to the value the charger really has
  enum class WriteState : uint8_t { DEBOUNCE, SENT, VERIFY };
  struct PendingWrite {
    uint16_t reg;
    uint32_t raw;
    WriteState state;
    uint32_t due_ms;   // DEBOUNCE: when to send. SENT/VERIFY: when the state times out
    bool has_seen;     // a value of the register was received since the write was sent
    uint32_t seen_raw;
  };
  bool queue_write_(uint16_t reg, uint32_t raw, uint32_t debounce_ms);
  PendingWrite *find_pending_(uint16_t reg);
  void drop_pending_(uint16_t reg);
  void publish_register_(uint16_t reg, uint32_t raw);  // number / switch entities backed by `reg`
  void on_register_value_(uint16_t reg, uint32_t raw);
  bool known_raw_(uint16_t reg, uint32_t &raw) const;

  vecan::VeCanHub *parent_{nullptr};
  uint8_t battery_instance_{0};
  uint8_t pv_instance_{1};

  float battery_voltage_{NAN};
  float battery_current_{NAN};
  float pv_voltage_{NAN};
  float pv_current_{NAN};

  std::set<uint16_t> poll_regs_;    // requested at every update()
  std::set<uint16_t> static_regs_;  // requested until answered once
  std::set<uint16_t> static_done_;
  std::map<uint16_t, uint32_t> known_values_;    // last value received for each writable register
  std::map<uint16_t, uint32_t> last_write_ms_;   // when each register was last written
  std::vector<PendingWrite> pending_;
  uint8_t on_mode_{1};  // value written for "charger on": the last non-off mode the charger reported
  std::set<uint16_t> unsupported_;  // registers the device answered with a NACK

  uint32_t boot_ms_{0};
  uint32_t last_rx_ms_{0};
  bool seen_traffic_{false};
  bool warned_silent_{false};
  bool warned_listen_only_{false};

#ifdef USE_SENSOR
  std::vector<SmartSolarSensor *> sensors_;
#endif
#ifdef USE_TEXT_SENSOR
  std::vector<SmartSolarTextSensor *> text_sensors_;
#endif
#ifdef USE_BINARY_SENSOR
  std::vector<SmartSolarBinarySensor *> binary_sensors_;
#endif
#ifdef USE_NUMBER
  std::vector<SmartSolarNumber *> numbers_;
#endif
#ifdef USE_SWITCH
  std::vector<SmartSolarSwitch *> switches_;
#endif
};

}  // namespace smartsolar
}  // namespace esphome
