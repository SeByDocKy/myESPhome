#include "jsy22x.h"
#include "esphome/core/log.h"

namespace esphome {
namespace jsy22x {

static const char *const TAG = "jsy22x";
static const uint8_t JSY22X_CMD_READ_IN_REGISTERS = 0x03;   // multiple registers
static const uint8_t JSY22X_CMD_WRITE_IN_REGISTERS = 0x10;
static const uint8_t JSY22X_REGISTER_SETTINGS_START = 0x04; // modbus address & databaud
static const uint8_t JSY22X_REGISTER_SETTINGS_COUNT = 0x01;  // 1 x 16-bit setting registers
static const uint8_t JSY22X_RESET_RESET_ENERGY_LB = 0x43; // 0x043;
static const uint16_t JSY22X_REGISTER_DATA_START = 0x0100;
static const uint8_t JSY22X_REGISTER_DATA_COUNT = 18;  // 20 x 16-bit data registers

void JSY22X::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up JSY22X..."); 
}

void JSY22X::on_modbus_data(const std::vector<uint8_t> &data) {
  if ((this->read_data_ == 1) & (data.size() < JSY22X_REGISTER_DATA_COUNT*2)) {
    ESP_LOGW(TAG, "Invalid size for JSY22X data!");
    return;
  }

  auto jsy22x_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 0]) << 8) | (uint16_t(data[i + 1]) << 0);
  };
  auto jsy22x_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(jsy22x_get_16bit(i + 0)) << 16) | (uint32_t(jsy22x_get_16bit(i + 2)) << 0);
  };
 
  if (this->read_data_ == 1){

    float voltage = static_cast<float>(jsy22x_get_16bit(0))/10000.0f;  
    float current = static_cast<float>(jsy22x_get_16bit(2))/10000.0f;
	float active_power = static_cast<float>(jsy22x_get_16bit(4))/10000.0f;
	float active_energy = static_cast<float>(jsy22x_get_16bit(6))/1000.0f;
	float power_factor = static_cast<float>(jsy22x_get_16bit(8))/1000.0f;   
	float reactive_power = static_cast<float>(jsy22x_get_16bit(10))/10000.0f;
	float reactive_energy = static_cast<float>(jsy22x_get_16bit(14))/100.0f;
    float frequency = static_cast<float>(jsy22x_get_16bit(16))/100.0f;
	float acdc_mode = static_cast<float>(jsy22x_get_16bit(18))/1.0f;
	  
    ESP_LOGD(TAG, "V=%.1f V, I=%.3f A, Pactive=%.1f W, Eactive=%.1f kWh, PF=%.2f, Preactive=%.1f VA, Ereactive=%.1f kVAh, F=%.1f Hz, ACDC mode=%d", voltage, current, active_power,
             active_energy, power_factor, reactive_power, reactive_energy, frequency, acdc_mode);

    if (this->voltage_sensor_ != nullptr)
      this->voltage_sensor_->publish_state(voltage);
    if (this->current_sensor_ != nullptr)
      this->current_sensor_->publish_state(current);
    if (this->active_power_sensor_ != nullptr)
      this->active_power_sensor_->publish_state(power);
    if (this->active_energy_sensor_ != nullptr)
      this->active_energy_sensor_->publish_state(active_energy);
    if (this->power_factor_sensor_ != nullptr)
      this->power_factor_sensor_->publish_state(power_factor);
	if (this->reactive_power_sensor_ != nullptr)
      this->reactive_power_sensor_->publish_state(reactive_power);
    if (this->reactive_energy_sensor_ != nullptr)
      this->reactive_energy_sensor_->publish_state(reactive_energy);
    if (this->frequency_sensor_ != nullptr)
      this->frequency_sensor_->publish_state(frequency);
    if (this->acdc_mode_sensor_ != nullptr)
      this->acdc_mode_sensor_->publish_state(acdc_mode); 
   }
  else if(this->read_data_ == 2){ // read 0x04 register
   if ( (data[0]>=1) & (data[0] <= 255) & (data[1]>=3) & (data[0] <= 8)){
	  this->current_address_ = data[0];
	  this->current_baudrate_= data[1];
	  ESP_LOGD(TAG, "JSY22X: Read 0x04 register with address=%d, baudrate = %d", this->current_address_, this->current_baudrate_);
	}
    else{
	  ESP_LOGD(TAG, "JSY22X: Read bad values from 0x04 : address=%d, baudrate = %d, keep current address =%d, baudrate %d", data[0], data[1] , this->current_address_ , this->current_baudrate_);
    }	
	this->read_data_ = 1;
  }
  else if(this->read_data_ == 3){ // write 0x04 register
   	if ( data.size() < 2){
	  ESP_LOGD(TAG, "JSY22X: failed to write new values 0x04 register with address=%d, baudrate = %d", this->new_address_, this->new_baudrate_);
	}
    else{
	  ESP_LOGD(TAG, "JSY22X: wrote new values into 0x04 register with address=%d, baudrate = %d", this->new_address_ , this->new_baudrate_);
    }	
	this->read_data_ = 1; 
  }
  else if(this->read_data_ == 4){ // Reset Energy1
   	if ( data.size() < 2){
	  ESP_LOGD(TAG, "JSY22X: failed to reset Energy1");
	}
    else{
	  ESP_LOGD(TAG, "JSY22X: successfully reset Energy1");
    }	
	this->read_data_ = 1; 
  }
  else if(this->read_data_ == 5){ // Reset Energy2
   	if ( data.size() < 2){
	  ESP_LOGD(TAG, "JSY22X: failed to reset Energy2");
	}
    else{
	  ESP_LOGD(TAG, "JSY22X: successfully reset Energy2");
    }	
	this->read_data_ = 1; 
  }  
}

void JSY22X::update() { this->send(JSY22X_CMD_READ_IN_REGISTERS, JSY22X_REGISTER_DATA_START , JSY22X_REGISTER_DATA_COUNT); }

void JSY22X::dump_config() {
  ESP_LOGCONFIG(TAG, "JSY22X:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Active Power", this->active_power_sensor_);
  LOG_SENSOR("", "Active Energy", this->active_energy_sensor_);
  LOG_SENSOR("", "Power Factor", this->power_factor_sensor_);
  LOG_SENSOR("", "Reactive Power", this->reactive_power_sensor_);
  LOG_SENSOR("", "Reactive Energy", this->reactive_energy_sensor_);
  LOG_SENSOR("", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("", "ACDC mode", this->acdc_mode_sensor_);
}

void JSY22X::read_register04() {
  this->read_data_ = 2;
  std::vector<uint8_t> cmd;
  cmd.push_back(this->address_); 
  cmd.push_back(JSY22X_CMD_READ_IN_REGISTERS);
  cmd.push_back(0x00);  
  cmd.push_back(JSY22X_REGISTER_SETTINGS_START);
  cmd.push_back(0x00);
  cmd.push_back(JSY22X_REGISTER_SETTINGS_COUNT);
  ESP_LOGD(TAG, "JSY22X: reading values from 0x04 register"); 
  this->send_raw(cmd);
}

void JSY22X::write_register04(uint8_t new_address , uint8_t new_baudrate) {
  if ((new_address>=1) & (new_address <= 255) & (new_baudrate>=3) & (new_baudrate <= 8)){
    this->read_data_ = 3;
	std::vector<uint8_t> cmd;
    cmd.push_back(0x00);  // broadcast address
    cmd.push_back(JSY22X_CMD_WRITE_IN_REGISTERS);
    cmd.push_back(0x00);  
    cmd.push_back(JSY22X_REGISTER_SETTINGS_START);
    cmd.push_back(0x00);
    cmd.push_back(0x01); 
    cmd.push_back(0x02);
    cmd.push_back(new_address);
    cmd.push_back(new_baudrate);
    ESP_LOGD(TAG, "JSY22X: writing values into 0x04 register: address=%d, baudrate = %d", new_address_, new_baudrate); 
    this->send_raw(cmd);
  } 
  else{
	 ESP_LOGD(TAG, "JSY22X: attempt to write bad values into 0x04 : Address=%d, baudrate = %d", new_address_, new_baudrate); 
  }
}

void JSY22X::reset_energy() {
  this->read_data_ = 4;	
  std::vector<uint8_t> cmd;
  cmd.push_back(this->address_);
  cmd.push_back(JSY22X_CMD_WRITE_IN_REGISTERS);
  cmd.push_back(0x00);  
  cmd.push_back(JSY22X_RESET_RESET_ENERGY_LB);
  cmd.push_back(0x00);
  cmd.push_back(0x02);   // 0x04
  cmd.push_back(0x04);   // 0x08
  
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x00);
/*  
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x00);
*/
  ESP_LOGD(TAG, "JSY22X: sending reset Energy command"); 
  this->send_raw(cmd);
}

}  // namespace jsy22x
}  // namespace esphome
