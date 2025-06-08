#include "dmtcp_output.h"

#include "esphome/core/log.h"

namespace esphome {
namespace dmtcp {
	
  static const char *const TAG = "dmtcp.output";
  
  void DMTCPOutput::dump_config() {
    ESP_LOGCONFIG(TAG, "DMTCP Output:");
  }
  void DMTCPOutput::write_state(float state) {
 
  }

	


 }
}