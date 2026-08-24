#include "pcm3k6w_button.h"

namespace esphome::pcm3k6w {

void PCM3K6WButton::press_action() { this->parent_->write_button(this->kind_); }

}  // namespace esphome::pcm3k6w
