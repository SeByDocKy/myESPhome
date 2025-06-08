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
	
	uint8_t request[] = {
        0x00, 0x01,  // Transaction ID
        0x00, 0x00,  // Protocol ID
        0x00, 0x06,  // Length
        this->unit_id_, // Unit ID
        0x03,        // Function Code (Read Holding Registers)
        (uint8_t)((this->start_modbus_address_ >> 8) & 0xFF),  // Start Address (High Byte)
        (uint8_t)(this->start_modbus_address_ & 0xFF),         // Start Address (Low Byte)
        (uint8_t)((this->nb_bytes_to_read_ >> 8) & 0xFF),  // (High Byte)
        (uint8_t)(this->nb_bytes_to_read_ & 0xFF),         // (Low Byte)       
    };
	  
  }
	
	
 }
}