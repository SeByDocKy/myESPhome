#include "jsy22x.h"
#include "esphome/core/log.h"

namespace esphome {
namespace jsy22x {

static const char *const TAG = "jsy22x";
static const uint8_t JSY22X_CMD_READ_IN_REGISTERS = 0x03;   // multiple registers
static const uint8_t JSY22X_CMD_WRITE_IN_REGISTERS = 0x10;
static const uint8_t JSY22X_REGISTER_SETTINGS_START = 0x04; // modbus address & databaud
static const uint8_t JSY22X_REGISTER_SETTINGS_COUNT = 0x01;  // 1 x 16-bit setting registers
static const uint8_t JSY22X_RESET_RESET_ENERGY_LB = 0x0C; // 0x043;
static const uint16_t JSY22X_REGISTER_DATA_START = 0x0100;
static const uint8_t JSY22X_REGISTER_DATA_COUNT = 16;  // 16 x 16-bit data registers

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
    float active_power_direction   = static_cast<float>(jsy22x_get_16bit(20)); //0 for positive, 1 for negative 
    float reactive_power_direction = static_cast<float>(jsy22x_get_16bit(22)); //0 for positive, 1 for negative
    float voltage = static_cast<float>(jsy22x_get_16bit(0))/10000.0f;
    uint16_t raw_current = jsy22x_get_16bit(2);  
    float current = ((1.0f - active_power_direction)*raw_current - active_power_direction*raw_current)/10000.0f;
    // float current = static_cast<float>(jsy22x_get_16bit(2))/10000.0f;
    uint16_t raw_active_power   = jsy22x_get_16bit(4);
    float active_power = ((1.0f - active_power_direction)*raw_active_power - active_power_direction*raw_active_power)/10000.0f;
    // float active_power = static_cast<float>(jsy22x_get_16bit(4))/10000.0f;
    uint16_t raw_reactive_power   = jsy22x_get_16bit(6);
    float reactive_power = ((1.0f - reactive_power_direction)*raw_reactive_power - reactive_power_direction*raw_reactive_power)/10000.0f;
    //float reactive_power = static_cast<float>(jsy22x_get_16bit(10))/10000.0f;
	
    uint16_t raw_apparent_power   = jsy22x_get_16bit(8);
    float apparent_power = ((1.0f - reactive_power_direction)*raw_apparent_power - reactive_power_direction*raw_apparent_power)/10000.0f;
    float power_factor = static_cast<float>(jsy22x_get_16bit(10))/1000.0f;
    float frequency = static_cast<float>(jsy22x_get_16bit(12))/100.0f;   	
	
    uint16_t raw_active_energy   = jsy22x_get_16bit(14);
    float active_energy = ((1.0f - active_power_direction)*raw_active_energy - active_power_direction*raw_active_energy)/1000.0f;
    // float active_energy = static_cast<float>(jsy22x_get_16bit(6))/1000.0f;
    uint16_t raw_reactive_energy   = jsy22x_get_16bit(16);
    float reactive_energy = ((1.0f - reactive_power_direction)*raw_reactive_energy - reactive_power_direction*raw_reactive_energy)/1000.0f;
    // float reactive_energy = static_cast<float>(jsy22x_get_16bit(14))/1000.0f;	    
    float acdc_mode = static_cast<float>(jsy22x_get_16bit(18))/1.0f;
    //	float active_power_direction = static_cast<float>(jsy22x_get_16bit(20))/1.0f;	  
    float pos_active_energy = static_cast<float>(jsy22x_get_16bit(24))/1000.0f;
    float neg_active_energy = static_cast<float>(jsy22x_get_16bit(26))/1000.0f;

    float pos_reactive_energy = static_cast<float>(jsy22x_get_16bit(28))/1000.0f;
    float neg_reactive_energy = static_cast<float>(jsy22x_get_16bit(30))/1000.0f;	
	
    ESP_LOGD(TAG, "modbus address=%d, baudrate=%d, V=%.1f V, I=%.3f A, P=%.1f W, Q=%.1f VAr, S=%.1f VA, PF=%.2f, F=%.1f Hz, EP=%.1f kWh, EQ=%.1f kVArh, ACDC mode=%1.0f, P_dir=%1.0f, Q_dir=%1.0f, E+=%.1f, E-=%.1f , Q+=%.1f, Q-=%.1f", int(this->address_), int(this->this->baud_rate_) , voltage, current, active_power,
             reactive_power, apparent_power, power_factor, frequency, active_energy, reactive_energy, acdc_mode, active_power_direction, reactive_power_direction,
	     pos_active_energy, neg_active_energy, pos_reactive_energy, neg_reactive_energy);

    if (this->voltage_sensor_ != nullptr)
      this->voltage_sensor_->publish_state(voltage);
    if (this->current_sensor_ != nullptr)
      this->current_sensor_->publish_state(current);
    if (this->active_power_sensor_ != nullptr)
      this->active_power_sensor_->publish_state(active_power);
    if (this->reactive_power_sensor_ != nullptr)
      this->reactive_power_sensor_->publish_state(reactive_power);
    if (this->apparent_power_sensor_ != nullptr)
      this->apparent_power_sensor_->publish_state(apparent_power);
    if (this->power_factor_sensor_ != nullptr)
      this->power_factor_sensor_->publish_state(power_factor);
    if (this->frequency_sensor_ != nullptr)
      this->frequency_sensor_->publish_state(frequency);
    if (this->active_energy_sensor_ != nullptr)
      this->active_energy_sensor_->publish_state(active_energy);
    if (this->reactive_energy_sensor_ != nullptr)
      this->reactive_energy_sensor_->publish_state(reactive_energy);
    if (this->acdc_mode_sensor_ != nullptr)
      this->acdc_mode_sensor_->publish_state(acdc_mode);
    if (this->active_power_direction_sensor_ != nullptr)
      this->active_power_direction_sensor_->publish_state(active_power_direction);
    if (this->reactive_power_direction_sensor_ != nullptr)
      this->reactive_power_direction_sensor_->publish_state(reactive_power_direction);
    if (this->pos_active_energy_sensor_ != nullptr)
      this->pos_active_energy_sensor_->publish_state(pos_active_energy);
    if (this->neg_active_energy_sensor_ != nullptr)
      this->neg_active_energy_sensor_->publish_state(neg_active_energy);
    if (this->pos_reactive_energy_sensor_ != nullptr)
      this->pos_reactive_energy_sensor_->publish_state(pos_reactive_energy);
    if (this->neg_reactive_energy_sensor_ != nullptr)
      this->neg_reactive_energy_sensor_->publish_state(neg_reactive_energy);
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
  LOG_SENSOR("", "Reactive Power", this->reactive_power_sensor_);
  LOG_SENSOR("", "Apparent Power", this->apparent_power_sensor_);
  LOG_SENSOR("", "Power Factor", this->power_factor_sensor_);
  LOG_SENSOR("", "Frequency", this->frequency_sensor_);
  
  LOG_SENSOR("", "Active Energy", this->active_energy_sensor_);
  LOG_SENSOR("", "Reactive Energy", this->reactive_energy_sensor_);
  
  LOG_SENSOR("", "ACDC mode", this->acdc_mode_sensor_);
  LOG_SENSOR("", "Active power direction", this->active_power_direction_sensor_);
  LOG_SENSOR("", "Reactive power direction", this->reactive_power_direction_sensor_);
  LOG_SENSOR("", "Positive active power", this->pos_active_energy_sensor_);
  LOG_SENSOR("", "Negative active power", this->neg_active_energy_sensor_);
  LOG_SENSOR("", "Positive reactive power", this->pos_reactive_energy_sensor_);
  LOG_SENSOR("", "Negative reactive power", this->neg_reactive_energy_sensor_);
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
