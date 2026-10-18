#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#include <string>

namespace esphome {
namespace auto_gen {

/// Prototype component creating ESPHome entities at runtime.
///
/// Entities live in fixed-size containers sized at compile time, so the Python
/// side reserves `max_sensors` extra slots (see __init__.py).  At runtime this
/// class allocates a Sensor, registers it with the Application and fires
/// `on_registered` so that Home Assistant can be asked to re-read the entities.
class AutoGenComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  /// Run late so that the API server and WiFi are configured first.
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_delay(uint32_t delay_ms) { this->delay_ms_ = delay_ms; }
  void set_max_sensors(uint8_t max_sensors) { this->max_sensors_ = max_sensors; }
  void set_sensor_name(const std::string &name) { this->sensor_name_ = name; }
  void set_uom_index(uint8_t uom_index) { this->uom_index_ = uom_index; }

  void add_on_registered_callback(std::function<void()> &&callback) {
    this->registered_callback_.add(std::move(callback));
  }

#ifdef USE_SENSOR
  /// Create and register a sensor at runtime.  Returns nullptr when no slot is left.
  sensor::Sensor *create_sensor(const std::string &name);
#endif

 protected:
  void create_demo_sensor_();

  uint32_t delay_ms_{10000};
  uint8_t max_sensors_{4};
  uint8_t uom_index_{0};
  uint8_t created_{0};
  std::string sensor_name_{"auto_gen_test"};
  float demo_value_{0.0f};
#ifdef USE_SENSOR
  sensor::Sensor *demo_sensor_{nullptr};
#endif
  CallbackManager<void()> registered_callback_{};
};

/// Trigger fired once a runtime entity has been registered.
class RegisteredTrigger : public Trigger<> {
 public:
  explicit RegisteredTrigger(AutoGenComponent *parent) {
    parent->add_on_registered_callback([this]() { this->trigger(); });
  }
};

}  // namespace auto_gen
}  // namespace esphome
