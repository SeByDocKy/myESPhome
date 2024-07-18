#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace solarpid {


class SOLARPIDComponent : public sensor::Sensor, public Component {
 public:
  void set_restore(bool restore) { restore_ = restore; }
  void set_time(time::RealTimeClock *time) { time_ = time; }
  void set_parent(Sensor *parent) { parent_ = parent; }
  void set_method(statistics_method method) { method_ = method; }
  
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void loop() override;


 protected:
  void process_new_state_(float state);

  ESPPreferenceObject pref_;
  time::RealTimeClock *time_;
  Sensor *parent_;

};

/*
template<typename... Ts> class STATISTICSresetaction : public Action<Ts...> {
 public:
  STATISTICSresetaction(STATISTICSComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->reset(); }

 protected:
  STATISTICSComponent *parent_;
};
*/
}  // namespace solarpid
}  // namespace esphome