#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/vecan/vecan.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace esphome {
namespace multirs {

// NOTE: the numeric values are mirrored in the platform __init__.py files
// (tests/check_kinds.py verifies that both sides agree).
enum SensorKind : uint8_t {
  // PGN 127508 (battery status)
  SENSOR_BATTERY_VOLTAGE = 0,
  SENSOR_BATTERY_CURRENT = 1,
  SENSOR_BATTERY_POWER = 2,
  SENSOR_BATTERY_TEMPERATURE = 3,
  // registers
  SENSOR_AC_IN_VOLTAGE = 4,
  SENSOR_AC_IN_CURRENT = 5,
  SENSOR_AC_IN_POWER = 6,
  SENSOR_AC_IN_APPARENT_POWER = 7,
  SENSOR_AC_IN_FREQUENCY = 8,
  SENSOR_AC_OUT_VOLTAGE = 9,
  SENSOR_AC_OUT_CURRENT = 10,
  SENSOR_AC_OUT_POWER = 11,
  SENSOR_AC_OUT_APPARENT_POWER = 12,
  SENSOR_AC_OUT_FREQUENCY = 13,
  SENSOR_PV_VOLTAGE = 14,
  SENSOR_PV_POWER = 15,
  SENSOR_ENERGY_TODAY = 16,
  SENSOR_ENERGY_YESTERDAY = 17,
  SENSOR_ENERGY_TOTAL = 18,
  SENSOR_INTERNAL_TEMPERATURE = 19,
};

enum TextSensorKind : uint8_t {
  TEXT_STATE = 0,
  TEXT_ERROR = 1,
  TEXT_FIRMWARE_VERSION = 2,
  TEXT_MODEL = 3,
  TEXT_SERIAL_NUMBER = 4,
};

enum SwitchKind : uint8_t {
  SWITCH_UPS_FUNCTION = 0,              // register 0xD067, bits 0-1
  SWITCH_GENERATOR_LOAD_MODERATION = 1, // register 0xD067, bits 2-3
  SWITCH_WEAK_AC_INPUT = 2,             // register 0xD067, bits 4-5
};

enum SelectKind : uint8_t {
  SELECT_MODE = 0,  // register 0x0200
};

#ifdef USE_SENSOR
class MultiRSSensor;
#endif
#ifdef USE_TEXT_SENSOR
class MultiRSTextSensor;
#endif
#ifdef USE_SWITCH
class MultiRSSwitch;
#endif
#ifdef USE_SELECT
class MultiRSSelect;
#endif

/// One Victron Multi RS Solar on VE.Can.
///
/// EXPERIMENTAL: written from the public VE.Can register list and from community reports, not tested on a real
/// device. Register presence and scales are assumptions. Use `scan_pages` / `log_unknown_registers` to see what
/// your unit really answers, and correct a scale with a `multiply:` sensor filter.
///
/// Battery values are broadcast as NMEA 2000 PGN 127508. Everything else is read from registers (VREGs), requested
/// every `poll_interval` (static ones only until they have been received).
class MultiRS : public PollingComponent, public vecan::VeCanDevice {
 public:
  void set_vecan(vecan::VeCanHub *hub) { this->parent_ = hub; }
  void set_battery_instance(uint8_t instance) { this->battery_instance_ = instance; }
  void set_write_enabled(bool enabled) { this->write_enabled_ = enabled; }
  void set_log_unknown_registers(bool enabled) { this->log_unknown_ = enabled; }
  void add_scan_page(uint16_t page) { this->scan_pages_.push_back(page); }

#ifdef USE_SENSOR
  void register_sensor(MultiRSSensor *sensor);
#endif
#ifdef USE_TEXT_SENSOR
  void register_text_sensor(MultiRSTextSensor *sensor);
#endif
#ifdef USE_SWITCH
  void register_switch(MultiRSSwitch *sw);
  void write_switch(uint8_t kind, bool state);
#endif
#ifdef USE_SELECT
  void register_select(MultiRSSelect *select);
  void write_select(uint8_t kind, size_t index);
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
  void request_(uint16_t reg);

  // --- register writes ---------------------------------------------------------------------------
  // Safety rules (settings live in the device's non-volatile memory):
  //  * nothing is written unless `write_enabled: true`
  //  * a value equal to the one the device already reports is never written
  //  * writes of one register are spaced by at least 10 s
  //  * the 0xD067 switches are read-modify-write: refused until the register has been read once
  //  * the device must confirm (broadcast of the new value); otherwise the register is read back and the entity
  //    returns to the value the device really has
  enum class WriteState : uint8_t { SENT, VERIFY };
  struct PendingWrite {
    uint16_t reg;
    uint32_t raw;
    WriteState state;
    uint32_t due_ms;
    bool has_seen;
    uint32_t seen_raw;
  };
  bool queue_write_(uint16_t reg, uint32_t raw);
  PendingWrite *find_pending_(uint16_t reg);
  void drop_pending_(uint16_t reg);
  void publish_register_(uint16_t reg, uint32_t raw);  // switch / select entities backed by `reg`
  void on_register_value_(uint16_t reg, uint32_t raw);
  bool known_raw_(uint16_t reg, uint32_t &raw) const;
  void republish_known_(uint16_t reg);

  vecan::VeCanHub *parent_{nullptr};
  uint8_t battery_instance_{0};
  bool write_enabled_{false};
  bool log_unknown_{false};
  std::vector<uint16_t> scan_pages_;
  uint8_t scan_rounds_{0};

  std::set<uint16_t> poll_regs_;
  std::set<uint16_t> static_regs_;
  std::set<uint16_t> static_done_;
  std::map<uint16_t, uint32_t> known_values_;
  std::map<uint16_t, uint32_t> last_write_ms_;
  std::vector<PendingWrite> pending_;
  std::set<uint16_t> unsupported_;

  uint32_t boot_ms_{0};
  bool seen_traffic_{false};
  bool warned_silent_{false};
  bool warned_listen_only_{false};

#ifdef USE_SENSOR
  std::vector<MultiRSSensor *> sensors_;
#endif
#ifdef USE_TEXT_SENSOR
  std::vector<MultiRSTextSensor *> text_sensors_;
#endif
#ifdef USE_SWITCH
  std::vector<MultiRSSwitch *> switches_;
#endif
#ifdef USE_SELECT
  std::vector<MultiRSSelect *> selects_;
#endif
};

}  // namespace multirs
}  // namespace esphome
