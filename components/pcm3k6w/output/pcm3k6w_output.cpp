#include "pcm3k6w_output.h"

namespace esphome::pcm3k6w {

void PCM3K6WOutput::write_state(float state) {
  // state is 0.0-1.0 (already inverted / power-scaled by the FloatOutput base).
  float current = this->min_value_ + state * (this->max_value_ - this->min_value_);
  this->parent_->write_output(this->kind_, current);
}

}  // namespace esphome::pcm3k6w
