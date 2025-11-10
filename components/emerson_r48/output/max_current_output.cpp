#include "max_current_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace emerson_r48 {

/* static const int8_t SET_VOLTAGE_FUNCTION = 0x0;
static const int8_t SET_CURRENT_FUNCTION = 0x3;
static const int8_t SET_INPUT_CURRENT_FUNCTION = 0x4;
 */
void EmersonR48MaxCurrentOutput::write_state(float value) {
	// this->parent_->write_float(value);
	this->parent_->set_level(value);
}

}  // namespace emerson_r48
}  // namespace esphome
