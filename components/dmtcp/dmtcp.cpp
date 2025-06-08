#include "dmtcp.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dmtcp {
  static const char *const TAG = "dmtcp";
  void DMTCPComponent::setup() {
   ESP_LOGCONFIG(TAG, "Setting up DEYE MODBUS TCP...");
  }
  float DMTCPComponent::get_setup_priority() const { return setup_priority::DATA; }
  void DMTCPComponent::update() { 
    //this->read_data_(); 
  }
  void DMTCPComponent::dump_config() {}
	
	
 }
}