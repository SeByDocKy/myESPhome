#include "pcm3k6w_switch.h"

namespace esphome::pcm3k6w {

void PCM3K6WSwitch::write_state(bool state) { this->parent_->write_switch(this->kind_, state); }

}  // namespace esphome::pcm3k6w
