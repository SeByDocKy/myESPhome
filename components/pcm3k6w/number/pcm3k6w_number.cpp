#include "pcm3k6w_number.h"

namespace esphome::pcm3k6w {

void PCM3K6WNumber::setup() {
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  float value;
  if (!this->pref_.load(&value)) value = this->initial_value_;
  this->publish_state(value);
  // Push the restored/default setpoint to the PCM once at boot; the CAN
  // readback (see PCM3K6WComponent::on_frame_) will correct it if the
  // device disagrees.
  this->parent_->write_number(this->kind_, value);
}

void PCM3K6WNumber::control(float value) {
  this->publish_state(value);
  this->parent_->write_number(this->kind_, value);
  this->pref_.save(&value);
}

}  // namespace esphome::pcm3k6w
