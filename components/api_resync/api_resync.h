#pragma once

#include "esphome/core/defines.h"

#ifdef USE_API

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
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

#ifdef USE_SELECT
/// Replaces the option list of a select at runtime, keeps the current selection
/// when its text is still available (otherwise falls back to `fallback_index`),
/// then requests a resync so Home Assistant refreshes the dropdown.
/// Does nothing if the option list is unchanged.
///
/// SelectTraits only stores `const char *` pointers, so the strings are owned by
/// this action and must outlive the traits: the action lives for the whole
/// program (actions are never destroyed).
template<typename... Ts> class SetSelectOptionsAction : public Action<Ts...>, public Parented<ApiResyncComponent> {
 public:
  void set_select(select::Select *sel) { this->select_ = sel; }
  void set_fallback_index(size_t index) { this->fallback_index_ = index; }
  void set_options_static(std::vector<std::string> options) {
    this->static_options_ = std::move(options);
    this->has_lambda_ = false;
  }
  void set_options_lambda(std::function<std::vector<std::string>(Ts...)> &&func) {
    this->options_f_ = std::move(func);
    this->has_lambda_ = true;
  }

  void play(const Ts &...x) override {
    std::vector<std::string> fresh = this->has_lambda_ ? this->options_f_(x...) : this->static_options_;
    if (fresh.empty()) {
      ESP_LOGE("api_resync", "'%s': empty option list rejected", this->select_->get_name().c_str());
      return;
    }
    if (this->same_options_(fresh))
      return;

    // Remember the current selection by text: the state is only an index and
    // the strings it points to are about to be replaced.
    const bool had_state = this->select_->has_state();
    std::string current;
    if (had_state) {
      auto cur = this->select_->current_option();
      current.assign(cur.c_str(), cur.size());
    }

    // Own the new strings first, then hand the pointers to the traits. Nothing
    // reads the options between these two statements (single threaded).
    this->storage_ = std::move(fresh);
    FixedVector<const char *> ptrs;
    ptrs.init(this->storage_.size());
    for (const auto &s : this->storage_)
      ptrs.push_back(s.c_str());
    this->select_->traits.set_options(ptrs);

    // The old active index may now be wrong or out of range: fix it right away
    // (publish_state validates the index and fires the state callbacks).
    if (had_state) {
      auto found = this->select_->index_of(current.c_str());
      if (found.has_value()) {
        auto active = this->select_->active_index();
        if (!active.has_value() || *active != *found)
          this->select_->publish_state(*found);
      } else {
        const size_t fb = this->fallback_index_ < this->storage_.size() ? this->fallback_index_ : 0;
        this->select_->publish_state(fb);
      }
    }

    this->parent_->request_resync();
  }

 protected:
  bool same_options_(const std::vector<std::string> &fresh) const {
    const auto &cur = this->select_->traits.get_options();
    if (cur.size() != fresh.size())
      return false;
    for (size_t i = 0; i < fresh.size(); i++) {
      if (strcmp(cur[i], fresh[i].c_str()) != 0)
        return false;
    }
    return true;
  }

  select::Select *select_{nullptr};
  std::vector<std::string> static_options_{};
  std::function<std::vector<std::string>(Ts...)> options_f_{};
  std::vector<std::string> storage_{};
  size_t fallback_index_{0};
  bool has_lambda_{false};
};
#endif  // USE_SELECT

}  // namespace api_resync
}  // namespace esphome

#endif  // USE_API
