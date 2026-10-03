#include "minipid.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::minipid {

static const char *const TAG = "minipid";
static const float coeffP = 0.001f;
static const float coeffI = 0.0001f;
static const float coeffD = 0.001f;

void MINIPIDComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MINIPIDComponent...");

  this->last_time_ = millis();
  this->integral_ = 0.0f;
  this->previous_output_ = 0.0f;
  this->previous_error_ = 0.0f;
  this->output_ = 0.0f;

  if (this->input_sensor_ != nullptr) {
    this->input_sensor_->add_on_state_callback([this](float state) {
      this->current_input_ = state;
      this->pid_update();
    });
    this->current_input_ = this->input_sensor_->state;
  }

  this->pid_computed_callback_.call();

  ESP_LOGVV(TAG, "setup: input=%3.2f, pid_mode = %d", this->current_input_, this->current_pid_mode_);
}

void MINIPIDComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "dump config:");
  ESP_LOGVV(TAG, "setup import part: input=%3.2f", this->current_input_);
  this->pid_computed_callback_.call();
}

void MINIPIDComponent::pid_update() {
  const uint32_t now = millis();
  float tmp;
  float alphaP, alphaI, alphaD, alpha;

  ESP_LOGVV(TAG, "Entered in pid_update()");
  ESP_LOGVV(TAG, "Current pid mode %d", this->current_pid_mode_);

#ifdef USE_SWITCH
  if (!this->current_manual_override_) {
#endif

    /*
     * PID disabled:
     * reset the dynamic state so a future activation starts cleanly.
     * This also prevents the integral term from accumulating while OFF.
     */
#ifdef USE_SWITCH
    if (!this->current_activation_) {
      this->integral_ = 0.0f;
      this->error_ = 0.0f;
      this->derivative_ = 0.0f;
      this->output_ = 0.0f;
      this->current_output_ = 0.0f;
      this->previous_output_ = 0.0f;
      this->previous_error_ = 0.0f;
      this->last_time_ = now;

      if (this->device_output_ != nullptr) {
        this->device_output_->set_level(0.0f);
      }

      ESP_LOGVV(TAG, "PID inactive: state reset, output=0");
      this->pid_computed_callback_.call();
      return;
    }
#endif

    /* Compute elapsed time safely. */
    this->dt_ = float(now - this->last_time_) / 1000.0f;

    this->error_ = -(this->current_setpoint_ - this->current_input_);

#ifdef USE_SWITCH
    if (this->current_reverse_) {
      this->error_ = -this->error_;
    }
#endif

    this->current_error_ = this->error_;

    /* Derivative term: never divide by zero. */
    if (this->dt_ > 0.0f) {
      this->derivative_ = (this->error_ - this->previous_error_) / this->dt_;
    } else {
      this->derivative_ = 0.0f;
    }

    /*
     * Integral with anti-windup.
     *
     * In incremental mode (pid_mode=false), output is:
     *   previous_output + P + I + D
     *
     * Do not keep integrating when the output is already saturated and
     * the current error would push it further into the same limit.
     * The integral is still allowed to unwind when the error changes sign.
     */
    float candidate_integral = this->integral_;

    if (this->dt_ > 0.0f) {
      tmp = this->error_ * this->dt_;
      if (!std::isnan(tmp) && std::isfinite(tmp)) {
        candidate_integral += tmp;
      }
    }

    /*
     * Bound the integral contribution to the available output range.
     * This is a second line of defence against excessive wind-up.
     */
    if (this->current_ki_ != 0.0f && std::isfinite(candidate_integral)) {
      const float output_span =
          std::max(0.0f, this->current_output_max_ - this->current_output_min_);
      const float integral_limit =
          output_span / (coeffI * std::fabs(this->current_ki_));

      if (std::isfinite(integral_limit) && integral_limit > 0.0f) {
        candidate_integral = std::min(
            std::max(candidate_integral, -integral_limit), integral_limit);
      }
    }

    /* P and D are independent of the integral candidate. */
    alphaP = coeffP * this->current_kp_ * this->error_;
    alphaD = coeffD * this->current_kd_ * this->derivative_;

    /* Previous output is used only in incremental mode. */
    float base_output = 0.0f;
    if (!std::isnan(this->previous_output_) && !this->current_pid_mode_) {
      base_output = this->previous_output_;
    }

    /* Test whether the new integral would drive the output further into saturation. */
    bool accept_integral = true;
    if (this->dt_ > 0.0f && this->current_ki_ != 0.0f) {
      const float candidate_alphaI = coeffI * this->current_ki_ * candidate_integral;
      const float candidate_output = base_output + alphaP + candidate_alphaI + alphaD;

      if (candidate_output > this->current_output_max_ && this->error_ > 0.0f) {
        accept_integral = false;
      } else if (candidate_output < this->current_output_min_ && this->error_ < 0.0f) {
        accept_integral = false;
      }
    }

    if (accept_integral) {
      this->integral_ = candidate_integral;
    }

    alphaI = coeffI * this->current_ki_ * this->integral_;
    alpha = alphaP + alphaI + alphaD;

    this->output_ = std::min(
        std::max(base_output + alpha, this->current_output_min_),
        this->current_output_max_);

    ESP_LOGVV(TAG, "previous output = %2.8f", base_output);
    ESP_LOGVV(TAG, "E = %3.2f, I = %3.2f, D = %3.2f, previous = %3.2f",
              this->error_, this->integral_, this->derivative_, base_output);

    ESP_LOGVV(TAG, "Pcoeff = %3.8f", alphaP);
    ESP_LOGVV(TAG, "Icoeff = %3.8f", alphaI);
    ESP_LOGVV(TAG, "Dcoeff = %3.8f", alphaD);

    ESP_LOGVV(TAG, "output_min = %1.2f", this->current_output_min_);
    ESP_LOGVV(TAG, "output_max = %1.2f", this->current_output_max_);

    ESP_LOGVV(TAG, "PIDcoeff = %3.8f", alpha);
    ESP_LOGVV(TAG, "Intermediate computed output=%1.6f", this->output_);

    ESP_LOGVV(
        TAG,
        "full pid update: setpoint %3.2f, Kp=%3.2f, Ki=%3.2f, Kd=%3.2f, output_min = %3.2f, output_max = %3.2f, previous_output_ = %3.2f, output_ = %3.2f, error_ = %3.2f, integral = %3.2f, derivative = %3.2f",
        this->current_setpoint_, coeffP * this->current_kp_,
        coeffI * this->current_ki_, coeffD * this->current_kd_,
        this->current_output_min_, this->current_output_max_,
        this->previous_output_, this->output_, this->error_, this->integral_,
        this->derivative_);

    ESP_LOGVV(TAG, "activation %d", this->current_activation_);

    ESP_LOGVV(TAG, "Final computed output=%1.6f", this->output_);

    /// Output must be in [0.0 - 1.0] ////
    if (this->device_output_ != nullptr && this->output_ != this->previous_output_) {
      this->device_output_->set_level(this->output_);
    }

    this->current_output_ = this->output_;
    this->pid_computed_callback_.call();

    this->last_time_ = now;
    this->previous_error_ = this->error_;
    this->previous_output_ = this->output_;

#ifdef USE_SWITCH
  }
#endif
}

}  // namespace esphome::minipid
