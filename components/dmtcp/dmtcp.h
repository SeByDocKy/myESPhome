#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <string>
#include <WiFiClient.h>


namespace esphome {
namespace dmtcp {
	
class DMTCPComponent : public PollingComponent{
  public:
    void setup() override;
    void update() override;
    void dump_config() override;
    float get_setup_priority() const override;
  
    void set_host_ip_address(std::string ip_address){this->ip_address_ = ip_address; }
    void set_host_port(uint16_t port){this->port_ = port; }
	void set_unit_id(uint8_t id){this->unit_id_ = id; }
	
	void deye_read_data();
  
  protected:
    std::string ip_address_;
	uint16_t port_;
	uint8_t unit_id_;
	uint16_t start_modbus_address_ = 0x006d;
	uint16_t nb_bytes_to_read = 0x0001;
	
  };
 }  // namespace dmtcp
}  // namespace esphome