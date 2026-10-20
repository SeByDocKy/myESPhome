#include "faikin_climate.h"

#include <cmath>
#include <cstdlib>

#include "esphome/core/log.h"

namespace esphome {
namespace faikin {

static const char *const TAG = "faikin.climate";

namespace {
constexpr const char *FAN_NAMES[] = {"1", "2", "3", "4", "5"};
}

void FaikinClimate::setup() {
  this->set_supported_custom_fan_modes({"1", "2", "3", "4", "5"});
  this->parent_->add_on_state_callback([this]() { this->update_from_hub_(); });
  this->update_from_hub_();
}

void FaikinClimate::dump_config() { LOG_CLIMATE("", "Faikin climate", this); }

climate::ClimateTraits FaikinClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION);
  traits.add_supported_mode(climate::CLIMATE_MODE_OFF);
  traits.add_supported_mode(climate::CLIMATE_MODE_HEAT_COOL);
  traits.add_supported_mode(climate::CLIMATE_MODE_COOL);
  traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);
  traits.add_supported_mode(climate::CLIMATE_MODE_DRY);
  traits.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_QUIET);
  traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH,
                                    climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL});
  traits.set_visual_min_temperature(s21::TEMP_MIN);
  traits.set_visual_max_temperature(s21::TEMP_MAX);
  traits.set_visual_temperature_step(0.5f);
  return traits;
}

void FaikinClimate::control(const climate::ClimateCall &call) {
  FaikinComponent *hub = this->parent_;

  if (call.get_mode().has_value()) {
    switch (*call.get_mode()) {
      case climate::CLIMATE_MODE_OFF:
        hub->control_power(false);
        break;
      case climate::CLIMATE_MODE_HEAT:
        hub->control_mode(s21::Mode::HEAT);
        break;
      case climate::CLIMATE_MODE_COOL:
        hub->control_mode(s21::Mode::COOL);
        break;
      case climate::CLIMATE_MODE_DRY:
        hub->control_mode(s21::Mode::DRY);
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        hub->control_mode(s21::Mode::FAN);
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
      case climate::CLIMATE_MODE_AUTO:
      default:
        hub->control_mode(s21::Mode::AUTO);
        break;
    }
  }

  if (call.get_target_temperature().has_value())
    hub->control_target_temperature(*call.get_target_temperature());

  if (call.get_fan_mode().has_value()) {
    switch (*call.get_fan_mode()) {
      case climate::CLIMATE_FAN_AUTO:
        hub->control_fan(s21::Fan::AUTO);
        break;
      case climate::CLIMATE_FAN_QUIET:
        hub->control_fan(s21::Fan::QUIET);
        break;
      default:
        ESP_LOGW(TAG, "Unsupported fan mode");
        break;
    }
  } else if (call.has_custom_fan_mode()) {
    const StringRef mode = call.get_custom_fan_mode();
    const int level = (mode.size() == 1) ? (mode.c_str()[0] - '0') : 0;
    if (level >= 1 && level <= 5)
      hub->control_fan(static_cast<s21::Fan>(static_cast<uint8_t>(s21::Fan::SPEED_1) + (level - 1)));
    else
      ESP_LOGW(TAG, "Unsupported custom fan mode");
  }

  if (call.get_swing_mode().has_value()) {
    switch (*call.get_swing_mode()) {
      case climate::CLIMATE_SWING_BOTH:
        hub->control_swing(true, true);
        break;
      case climate::CLIMATE_SWING_VERTICAL:
        hub->control_swing(false, true);
        break;
      case climate::CLIMATE_SWING_HORIZONTAL:
        hub->control_swing(true, false);
        break;
      default:
        hub->control_swing(false, false);
        break;
    }
  }
}

void FaikinClimate::update_from_hub_() {
  const s21::State &st = this->parent_->get_state();
  if ((st.known & s21::GROUP_D1) == 0)
    return;

  if (!st.power) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    switch (st.mode) {
      case s21::Mode::HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case s21::Mode::COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case s21::Mode::DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case s21::Mode::FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      default:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
    }
  }

  if (!std::isnan(st.target_temp))
    this->target_temperature = st.target_temp;
  if (!std::isnan(st.home))
    this->current_temperature = st.home;

  switch (st.fan) {
    case s21::Fan::AUTO:
      this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
      break;
    case s21::Fan::QUIET:
      this->set_fan_mode_(climate::CLIMATE_FAN_QUIET);
      break;
    default: {
      const int idx = static_cast<int>(st.fan) - static_cast<int>(s21::Fan::SPEED_1);
      if (idx >= 0 && idx < 5)
        this->set_custom_fan_mode_(FAN_NAMES[idx]);
      break;
    }
  }

  if (st.swing_h && st.swing_v)
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  else if (st.swing_v)
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  else if (st.swing_h)
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  else
    this->swing_mode = climate::CLIMATE_SWING_OFF;

  // The action is inferred: a non-zero compressor value means the compressor is running. In auto
  // mode the direction is guessed from the target and the room temperature.
  if (!st.power) {
    this->action = climate::CLIMATE_ACTION_OFF;
  } else if (st.mode == s21::Mode::DRY) {
    this->action = climate::CLIMATE_ACTION_DRYING;
  } else if (st.mode == s21::Mode::FAN) {
    this->action = climate::CLIMATE_ACTION_FAN;
  } else if (st.comp > 0) {
    bool heating = (st.mode == s21::Mode::HEAT);
    if (st.mode == s21::Mode::AUTO && !std::isnan(st.target_temp) && !std::isnan(st.home))
      heating = st.target_temp > st.home;
    this->action = heating ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_COOLING;
  } else {
    this->action = climate::CLIMATE_ACTION_IDLE;
  }

  this->publish_state();
}

}  // namespace faikin
}  // namespace esphome
