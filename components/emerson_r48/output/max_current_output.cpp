#include "max_current_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace emerson_r48 {

void EmersonR48MaxCurrentOutput::write_state(float value) {
	this->parent_->set_max_output_current((value*100.0f),false);
	ESP_LOGVV("R48", "max output current new value %f", (value*100.0f));
}

}  // namespace emerson_r48
}  // namespace esphome
