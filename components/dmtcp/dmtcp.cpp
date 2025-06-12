#include "dmtcp.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dmtcp {
  static const char *const TAG = "dmtcp";
  static const uint8_t SIZE_RESPONSE = 255;
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
	else {
	  ESP_LOGV(TAG, "connected to %s:%d" , this->ip_address_.c_str(), this->port_);
	}
		
    }
	
	uint8_t request[] = {
        0x00, 0x01,           // Transaction ID
        0x00, 0x00,           // Protocol ID
        0x00, 0x06,           // Length
        this->read_unit_id_,       // Unit ID
        this->read_fcn_code_, // Function Code (Read Holding Registers)
        (uint8_t)((this->start_modbus_address_ >> 8) & 0xFF),  // Start Address (High Byte)
        (uint8_t)(this->start_modbus_address_ & 0xFF),         // Start Address (Low Byte)
        (uint8_t)((this->nb_bytes_to_read_ >> 8) & 0xFF),      // (High Byte)
        (uint8_t)(this->nb_bytes_to_read_ & 0xFF),             // (Low Byte)       
    };
	ESP_LOGV(TAG, "request size %d" , (uint8_t)sizeof(request));
	ESP_LOGV(TAG, "request %d:%d : %d:%d : %d:%d : %d:%d : %d:%d : %d:%d : %d:%d" , request[0],request1] , request[2],request[3] , request[4],request[5] , request[6],request[7] , request[8],request[9] , request[10],request[11] , request[12],request[13]);
	
	
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

    if (response[7] != this->read_fcn_code_) {
      ESP_LOGE("modbus_tcp", "Unexpected function code: 0x%02X", response[7]);
      return;
    }
	
	// Decode value based on byte order
    this->current_pv1_voltage_ = static_cast<float>(( response[9] << 8) | response[10])*0.1f;
	  
  }
	
 }
}