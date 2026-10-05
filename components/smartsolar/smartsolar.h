#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/vecan/vecan.h"

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
};

enum TextSensorKind : uint8_t {
  TEXT_STATE = 0,
  TEXT_ERROR = 1,
  TEXT_FIRMWARE_VERSION = 2,
  TEXT_MODEL = 3,
  TEXT_SERIAL_NUMBER = 4,
};

enum BinarySensorKind : uint8_t {
  BINARY_RELAY = 0,
  BINARY_ALARM = 1,
  BINARY_LOW_VOLTAGE = 2,
  BINARY_HIGH_VOLTAGE = 3,
  BINARY_SOLAR_ACTIVITY = 4,
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

  void setup() override;
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
};

}  // namespace smartsolar
}  // namespace esphome
