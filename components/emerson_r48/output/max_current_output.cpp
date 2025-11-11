#include "max_current_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace emerson_r48 {

void EmersonR48MaxCurrentOutput::write_state(float value) {
	this->parent_->set_max_output_current(value*100,false);
	ESP_LOGI("R48", "max output current new value %f %", value*100);
}

}  // namespace emerson_r48
}  // namespace esphome
