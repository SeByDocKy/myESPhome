#include "dmtcp.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dmtcp {
  static const char *const TAG = "dmtcp";
  void DMTCPComponent::setup() {
   ESP_LOGCONFIG(TAG, "Setting up DEYE MODBUS TCP...");
  }
  float DMTCPComponent::get_setup_priority() const { return setup_priority::DATA; }
  void DMTCPComponent::update() { this->deye_read_data(); }
  void DMTCPComponent::dump_config() {}
  
  void DMTCPComponent::deye_read_data(){
	WiFiClient client;
	if (!client.connect(this->ip_address_.c_str(), this->port_)) {
      ESP_LOGE("modbus_tcp", "Failed to connect to Modbus server %s:%d", this->ip_address_.c_str(), this->port_);
      return;
    }
	  
  }
	
	
 }
}