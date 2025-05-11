#include "offsr_switch.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace offsr {
	
static const char *const TAG = "offsr.switch";

void OFFSRSwitch::write_state(bool state) {
  if (this->activation_switch_ != nullptr) {	
       this->activation_switch_.publish_state(state);
       this->parent_->set_activation(state);
  }
  if (this->manual_override_switch_ != nullptr) {	
       this->manual_override_switch_.publish_state(state);
       this->parent_->set_manual_override(state);
  }
}



} // offsr
} // esphome
