#include "dmtcp.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dmtcp {
  static const char *const TAG = "dmtcp";
  void DMTCPomponent::setup() {
   ESP_LOGCONFIG(TAG, "Setting up DEYE MODBUS TCP...");
  }
  float DMTCPomponent::get_setup_priority() const { return setup_priority::DATA; }
  void DMTCPomponent::update() { 
    //this->read_data_(); 
  }
  void DMTCPomponent::dump_config() {
	  
  }
	
	
 }
}