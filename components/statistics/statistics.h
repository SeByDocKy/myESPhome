#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace statistics {

enum statistics_method {
  STATISTICS_METHODS_MIN = 0,
  STATISTICS_METHODS_MAX,
  STATISTICS_METHODS_MEAN,
};

class STATISTICSComponent : public sensor::Sensor, public Component {
 public:
  void set_restore(bool restore) { restore_ = restore; }
  void set_time(time::RealTimeClock *time) { time_ = time; }
  void set_parent(Sensor *parent) { parent_ = parent; }
  void set_method(statistics_method method) { method_ = method; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void loop() override;

  void publish_state_and_save(float state);
  void reset();

 protected:
  void process_new_state_(float state);

  ESPPreferenceObject pref_;
  time::RealTimeClock *time_;
  Sensor *parent_;
  statistics_method method_;
  uint16_t last_day_of_year_{};
  uint32_t last_update_{0};
  uint32_t last_n_{0};
  bool restore_;
  float last_statistics_state_{0.0f};
};

template<typename... Ts> class STATISTICSresetaction : public Action<Ts...> {
 public:
  STATISTICSresetaction(STATISTICSComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->reset(); }

 protected:
  STATISTICSComponent *parent_;
};

}  // namespace statistics
}  // namespace esphome