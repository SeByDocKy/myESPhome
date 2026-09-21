#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/uart/uart.h"
#include <vector>
#include <utility>
#include <string>

namespace esphome {
namespace ez1m {

// Hardware model, set via the hub's optional `model:` YAML key. Drives the
// maximum output power accepted by set_power_limit()/turn_on() and the
// default upper bound of the `power_limit` number (see number/__init__.py).
// Always available (not guarded by any USE_xxx) since the hub itself always
// exists.
enum class EZ1MModel {
  EZ1M,  // 800 W
  EZ1H,  // 960 W
  EZ1D,  // 1800 W
};

#ifdef USE_SENSOR
enum class EZ1MSensorType {
  CH1_DC_VOLTAGE,
  CH2_DC_VOLTAGE,
  CH1_DC_CURRENT,
  CH2_DC_CURRENT,
  CH1_DC_POWER,
  CH2_DC_POWER,
  TOTAL_DC_POWER,
  AC_POWER,
  GRID_FREQUENCY,
  TEMPERATURE,
  DAILY_ENERGY,
  CH1_SESSION_ENERGY,
  CH2_SESSION_ENERGY,
  LIFETIME_ENERGY,
  INVERTER_UPTIME,
  POWER_LIMIT_READBACK,
};
class EZ1MSensor;
#endif

#ifdef USE_TEXT_SENSOR
enum class EZ1MTextSensorType {
  INVERTER_STATE,
  DSP_VERSION,
};
class EZ1MTextSensor;
#endif

#ifdef USE_NUMBER
enum class EZ1MNumberType {
  POWER_LIMIT,
  TOTAL_ENERGY,
};
class EZ1MNumber;
#endif

#ifdef USE_SWITCH
class EZ1MSwitch;
#endif

class EZ1MComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // --- Config setters (codegen) ---
  void set_model(EZ1MModel model);
  EZ1MModel get_model() const { return this->model_; }
  float get_max_power() const { return this->max_power_; }
  void set_dc_voltage_divisor(float d) { this->dc_voltage_divisor_ = d; }
  void set_dc_current_divisor(float d) { this->dc_current_divisor_ = d; }
  void set_grid_frequency_divisor(float d) { this->grid_frequency_divisor_ = d; }

  // --- Entity registration (called from each platform's to_code, only
  //     compiled when that platform is actually used somewhere in the
  //     config -- hence the USE_xxx guards) ---
#ifdef USE_SENSOR
  void register_sensor(EZ1MSensor *sensor, EZ1MSensorType type);
#endif
#ifdef USE_TEXT_SENSOR
  void register_text_sensor(EZ1MTextSensor *sensor, EZ1MTextSensorType type);
#endif
#ifdef USE_NUMBER
  void set_power_limit_number(EZ1MNumber *number) { this->power_limit_number_ = number; }
  void set_total_energy_number(EZ1MNumber *number) { this->total_energy_number_ = number; }
#endif
#ifdef USE_SWITCH
  void set_onoff_switch(EZ1MSwitch *sw) { this->onoff_switch_ = sw; }
#endif

  // --- Commands: always available. Called by EZ1MNumber / EZ1MSwitch /
  //     EZ1MOutput -- none of which need the *other* optional platforms to
  //     be present, so these stay outside any USE_xxx guard. ---
  void set_power_limit(float watts);
  void turn_on(float watts);
  void turn_off();
  float get_power_limit_value() const;

  // --- Lifetime energy (RAM accumulator persisted to flash) ---
  void set_lifetime_energy(float kwh);
  float get_lifetime_energy() const { return this->total_kwh_; }

  // --- Startup power limit (persisted to flash, applied at setup()) ---
  // Set by pressing one of the `button` platform's set_power_min/
  // set_power_max entities; applied automatically every time the hub
  // (re)boots, so the inverter always comes back up at a known power limit
  // instead of 0 W/undefined -- e.g. after an unexpected power loss
  // overnight or during low sun. Also applies the new value to the
  // inverter immediately, not just on the next boot.
  void set_startup_power_limit(float watts);
  float get_startup_power_limit() const { return this->startup_power_limit_; }

 protected:
#ifdef USE_SENSOR
  void publish_sensor_(EZ1MSensorType type, float value);
#endif
#ifdef USE_TEXT_SENSOR
  void publish_text_sensor_(EZ1MTextSensorType type, const std::string &value);
#endif
  void send_poll_request_();
  void handle_frame_(const uint8_t *bytes, size_t frame_len);
  uint16_t watts_to_raw_(float watts) const;
  void save_lifetime_energy_();
  void load_lifetime_energy_();
  void save_startup_power_limit_();
  void load_startup_power_limit_();

  std::vector<uint8_t> rx_buffer_;
#ifdef USE_SENSOR
  std::vector<std::pair<EZ1MSensorType, EZ1MSensor *>> sensors_;
#endif
#ifdef USE_TEXT_SENSOR
  std::vector<std::pair<EZ1MTextSensorType, EZ1MTextSensor *>> text_sensors_;
#endif
#ifdef USE_NUMBER
  EZ1MNumber *power_limit_number_{nullptr};
  EZ1MNumber *total_energy_number_{nullptr};
#endif
#ifdef USE_SWITCH
  EZ1MSwitch *onoff_switch_{nullptr};
#endif

  EZ1MModel model_{EZ1MModel::EZ1M};
  float max_power_{800.0f};

  float dc_voltage_divisor_{50.0f};
  float dc_current_divisor_{88.0f};
  float grid_frequency_divisor_{27.32f};

  float total_kwh_{0.0f};
  float prev_daily_{-1.0f};
  ESPPreferenceObject pref_;

  // -1 == "not yet loaded/never saved"; load_startup_power_limit_() replaces
  // it with the saved value, or with this model's max power on first boot.
  float startup_power_limit_{-1.0f};
  ESPPreferenceObject startup_power_limit_pref_;

  // The very first set_power_limit() sent from setup() can be sent before
  // the inverter's own UART receiver has finished its own power-on sequence
  // and gets silently ignored -- confirmed by the real hardware readback
  // still showing the inverter's own power-on default right after boot. So
  // instead of a single fire-and-forget command, the startup limit is
  // resent on every poll cycle (see update()/handle_frame_()) until the
  // hardware readback actually confirms it took effect.
  //
  // This is gated by a wall-clock deadline (millis()) rather than a fixed
  // retry count: it's the inverter's own boot time that's variable (a cold
  // boot after a real power-cycle can take noticeably longer to come up
  // than a soft ESP restart), and it's entirely decoupled from the ESP32's
  // WiFi/API connection state, which runs in parallel and never gates the
  // UART traffic to the inverter.
  bool startup_limit_pending_{false};
  uint32_t startup_limit_deadline_{0};
};

}  // namespace ez1m
}  // namespace esphome
