#include "max_current_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace emerson_r48 {

void EmersonR48MaxCurrentOutput::write_state(float value) {
	this->parent_->set_max_output_current(value);
	// ESP_LOGI("
}

}  // namespace emerson_r48
}  // namespace esphome
