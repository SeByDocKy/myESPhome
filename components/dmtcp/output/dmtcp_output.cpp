#include "dmtcp_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dmtcp {
	
  static const char *const TAG = "dmtcp.output";
  
  void DMTCPOutput::dump_config() {
    ESP_LOGCONFIG(TAG, "DMTCP Output:");
  }
  void DMTCPOutput::write_state(float state) {
   	WiFiClient client;
	
	if (!client.connect((this->parent_->get_ip_address()).c_str(), this->parent_->get_port())) {
      ESP_LOGE("modbus_tcp", "Failed to connect to Modbus server %s:%d", (this->parent_->get_ip_address()).c_str(), this->parent_->get_port());
      return;
    }
	
	uint8_t request[] = {
        0x00, 0x01,            // Transaction ID
        0x00, 0x00,            // Protocol ID
        0x00, 0x06,            // Length
        this->write_unit_id_,  // Unit ID
        this->write_fcn_code_, // Function Code (write Holding Registers)
        (uint8_t)((this->start_modbus_address_ >> 8) & 0xFF),  // Start Address (High Byte)
        (uint8_t)(this->start_modbus_address_ & 0xFF),         // Start Address (Low Byte)
        (uint8_t)((this->nb_bytes_to_read_ >> 8) & 0xFF),      // (High Byte)
        (uint8_t)(this->nb_bytes_to_read_ & 0xFF),             // (Low Byte)       
    };
	
	client.write(request, sizeof(request));
    delay(100);
	
	uint8_t response[SIZE_RESPONSE];
    size_t response_len = client.read(response, sizeof(response));
    if (response_len < 9) {
      ESP_LOGE("modbus_tcp", "Invalid response length: %d", response_len);
      return;
    }
	// Debugging: Print raw response data
    ESP_LOGD("modbus_tcp", "Raw Response Data:");
    for (size_t i = 0; i < response_len; i++) {
      ESP_LOGD("modbus_tcp", "Byte %d: 0x%02X", i, response[i]);
    }
    if (response[7] != this->write_fcn_code_) {
      ESP_LOGE("modbus_tcp", "Unexpected function code: 0x%02X", response[7]);
      return;
    } 
  }
 }
}