#include "max_current_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace emerson_r48 {

void EmersonR48MaxCurrentOutput::write_state(float value) {
	this->parent_->set_max_input_current(value);
}

}  // namespace emerson_r48
}  // namespace esphome
