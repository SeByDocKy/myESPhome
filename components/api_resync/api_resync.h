#pragma once

#include "esphome/core/defines.h"

#ifdef USE_API

#include <cstdint>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

namespace esphome {
namespace api_resync {

/// Forces Home Assistant to re-run ListEntities by asking every connected API
/// client to reconnect. Requests are debounced (delay) and rate limited
/// (min_interval): a request that arrives too early is postponed, never lost.
class ApiResyncComponent : public Component {
 public:
  void set_min_interval(uint32_t ms) { this->min_interval_ms_ = ms; }
  void set_delay(uint32_t ms) { this->delay_ms_ = ms; }

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  /// Schedule a resync. Several calls within `delay` collapse into one.
  void request_resync();

 protected:
  void do_resync_();
  void send_disconnect_requests_();

  uint32_t min_interval_ms_{5000};
  uint32_t delay_ms_{500};
  uint32_t last_resync_ms_{0};
  bool has_resynced_{false};
};

template<typename... Ts> class ResyncAction : public Action<Ts...>, public Parented<ApiResyncComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->request_resync(); }
};

#ifdef USE_NUMBER
/// Changes min/max/step of a number at runtime, optionally clamps its current
/// state into the new range, then requests a resync so Home Assistant refreshes
/// the slider limits. Does nothing if the range is unchanged.
template<typename... Ts> class SetNumberRangeAction : public Action<Ts...>, public Parented<ApiResyncComponent> {
 public:
  void set_number(number::Number *num) { this->number_ = num; }
  void set_clamp_state(bool clamp) { this->clamp_state_ = clamp; }
  TEMPLATABLE_VALUE(float, min_value)
  TEMPLATABLE_VALUE(float, max_value)
  void set_step(std::function<float(Ts...)> &&func) {
    this->step_ = std::move(func);
    this->has_step_ = true;
  }
  void set_step(float value) {
    this->step_ = [value](Ts...) { return value; };
    this->has_step_ = true;
  }

  void play(const Ts &...x) override {
    auto &t = this->number_->traits;
    const float new_min = this->min_value_.value(x...);
    const float new_max = this->max_value_.value(x...);
    const float new_step = this->has_step_ ? this->step_(x...) : t.get_step();

    if (t.get_min_value() == new_min && t.get_max_value() == new_max && t.get_step() == new_step)
      return;

    t.set_min_value(new_min);
    t.set_max_value(new_max);
    t.set_step(new_step);

    if (this->clamp_state_ && this->number_->has_state()) {
      const float cur = this->number_->state;
      float clamped = cur;
      if (clamped < new_min)
        clamped = new_min;
      if (clamped > new_max)
        clamped = new_max;
      if (clamped != cur)
        this->number_->publish_state(clamped);
    }

    this->parent_->request_resync();
  }

 protected:
  number::Number *number_{nullptr};
  std::function<float(Ts...)> step_{};
  bool has_step_{false};
  bool clamp_state_{true};
};
#endif  // USE_NUMBER

}  // namespace api_resync
}  // namespace esphome

#endif  // USE_API
