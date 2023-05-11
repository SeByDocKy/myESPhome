#include "statistics.h"
#include "esphome/core/log.h"

namespace esphome {
namespace statistics {

static const char *const TAG = "statistics";

void STATISTICSComponent::setup() {
  float initial_value = 0;

  if (this->restore_) {
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    this->pref_.load(&initial_value);
  }
  this->publish_state_and_save(initial_value);

  this->last_update_ = millis();

  this->parent_->add_on_state_callback([this](float state) { this->process_new_state_(state); });
}

void STATISTICSComponent::dump_config() { LOG_SENSOR("", "statistics", this); }

void STATISTICSComponent::loop() {
  auto t = this->time_->now();
  if (!t.is_valid())
    return;

  if (this->last_day_of_year_ == 0) {
    this->last_day_of_year_ = t.day_of_year;
    return;
  }

  if (t.day_of_year != this->last_day_of_year_) {
/*     this->last_day_of_year_ = t.day_of_year;
    this->stats_ = 0;
	this->last_n_ = 0;
    this->publish_state_and_save(0);
 */	this->reset();
  }
}

void STATISTICSComponent::reset() {
	auto t = this->time_->now();
	this->last_day_of_year_ = t.day_of_year;
    this->stats_ = 0;
	this->last_n_ = 0;
    this->publish_state_and_save(0);
}

void STATISTICSComponent::publish_state_and_save(float state) {
  this->stats_ = state;
  this->publish_state(state);
  if (this->restore_) {
    this->pref_.save(&state);
  }
}

void STATISTICSComponent::process_new_state_(float state) {
  if (std::isnan(state))
    return;
  const uint32_t now = millis();
  const float old_state = this->last_statistics_state_;
  const float new_state = state;
  uint32_t n = this->last_n_;
  float value = 0.0f;
  float ninv = 1.0f;
  switch (this->method_) {
    case STATISTICS_METHODS_MIN:
      value = std::min(old_state , new_state);
      break;
    case STATISTICS_METHODS_MAX:
      value = std::max(old_state , new_state);
      break;
    case STATISTICS_METHODS_MEAN:
	  n++;
	  ninv = 1.0f/n;
      value = ninv * new_state + (1 - ninv) * old_state;
      break;
  }
  this->last_statistics_state_ = new_state;
  this->last_update_ = now;
  this->last_n_ = n;
  this->publish_state_and_save(value);
}

}  // namespace statistics
}  // namespace esphome