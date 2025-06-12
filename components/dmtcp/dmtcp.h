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
	void set_unit_id(uint8_t id){this->read_unit_id_ = id; }
	
	void deye_read_data();
	
	void add_on_dmtcp_callback(std::function<void()> &&callback) {
    dmtcp_callback_.add(std::move(callback));
  }
	
#ifdef USE_SENSOR
    float get_pv1_voltage(void) { return this->current_pv1_voltage_; }
#endif

#ifdef USE_OUTPUT
    std::string get_ip_address(void){return this->ip_address_;}
	uint16_t get_port(void){return this->port_;}
#endif
	
  
  protected:
    std::string ip_address_;
	uint16_t port_;
	uint8_t read_unit_id_ = 0x08;
	uint8_t read_fcn_code_= 0x03;
	uint16_t start_modbus_address_ = 0x006d;
	uint16_t nb_bytes_to_read_ = 0x0002;
	
	float current_pv1_voltage_;
	
	CallbackManager<void()> dmtcp_callback_;
	
  };
 }  // namespace dmtcp
}  // namespace esphome