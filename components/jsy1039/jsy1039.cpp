#include "jsy1039.h"
#include "esphome/core/log.h"

namespace esphome {
namespace jsy1039 {

static const char *const TAG = "jsy1039";
static const uint8_t JSY1039_CMD_READ_IN_REGISTERS = 0x03;   // multiple registers
static const uint8_t JSY1039_CMD_WRITE_IN_REGISTERS = 0x10;
static const uint8_t JSY1039_REGISTER_SETTINGS_START = 0x04; // modbus address & databaud
static const uint8_t JSY1039_REGISTER_SETTINGS_COUNT = 0x01;  // 1 x 16-bit setting registers
static const uint8_t JSY1039_RESET_ENERGY_LB = 0x0C; // 0x0104;
static const uint16_t JSY1039_REGISTER_DATA_START = 0x048;  
static const uint8_t JSY1039_REGISTER_DATA_COUNT = 9;  // 20 x 16-bit data registers

void JSY1039::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up JSY1039..."); 
}

void JSY1039::on_modbus_data(const std::vector<uint8_t> &data) {
	
  float sign;
  if ((this->read_data_ == 1) & (data.size() < JSY1039_REGISTER_DATA_COUNT*2)) {
    ESP_LOGW(TAG, "Invalid size for JSY1039 data!");
    return;
  }

  auto jsy1039_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 0]) << 8) | (uint16_t(data[i + 1]) << 0);
  };
  auto jsy1039_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(jsy1039_get_16bit(i + 0)) << 16) | (uint32_t(jsy1039_get_16bit(i + 2)) << 0);
  };
	
  if (this->read_data_ == 1){
    float voltage = static_cast<float>(jsy1039_get_16bit(0))/100.0f;  // max 655.35 V
	
	uint16_t raw_current = jsy1039_get_16bit(2);
	if ((raw_current >> 15) & 0x01){
		sign = -1.0f;}
	else{
		  sign = 1.0f;
	}
	float current = static_cast<float>(raw_current & 0x7FFF)/100.0f;  
    // float current = static_cast<float>(jsy1039_get_16bit(2))/100.0f;
    float power   = static_cast<float>(jsy1039_get_16bit(4))*10.0f*sign;  //   10W of precision, better to collect power with voltage, current and power_factor
    float pos_energy = static_cast<float>(jsy1039_get_32bit(6))/100.0f; // max 42 949 673 kWh
    float neg_energy = static_cast<float>(jsy1039_get_32bit(10))/100.0f; // max 42 949 673 kWh
    float power_factor = static_cast<float>(jsy1039_get_16bit(14))/1000.0f;   // max 65.535
    float frequency = static_cast<float>(jsy1039_get_16bit(16))/100.0f;  // max 655.35 Hz
	// float power   = voltage*current*power_factor;
  
    ESP_LOGVV(TAG, "modbus address=%d, V=%.1f V, I=%.3f A, P=%2.1f W, E+=%.1f kWh , E-=%.1f kWh, F=%.1f Hz, PF=%.2f", int(this->address_), voltage, current, power, pos_energy, neg_energy, frequency, power_factor);

    if (this->voltage_sensor_ != nullptr)
      this->voltage_sensor_->publish_state(voltage);
    if (this->current_sensor_ != nullptr)
      this->current_sensor_->publish_state(current);
    if (this->power_sensor_ != nullptr)
      this->power_sensor_->publish_state(power);
    if (this->pos_energy_sensor_ != nullptr)
      this->pos_energy_sensor_->publish_state(pos_energy);
    if (this->neg_energy_sensor_ != nullptr)
      this->neg_energy_sensor_->publish_state(neg_energy);
    if (this->frequency_sensor_ != nullptr)
      this->frequency_sensor_->publish_state(frequency);
    if (this->power_factor_sensor_ != nullptr)
      this->power_factor_sensor_->publish_state(power_factor);
  
  }
  else if(this->read_data_ == 2){ // read 0x04 register
   if ( (data[0]>=1) & (data[0] <= 255) & (data[1]>=3) & (data[0] <= 8)){
	  this->current_address_ = data[0];
	  this->current_baudrate_= data[1];
	  ESP_LOGD(TAG, "JSY1039: Read 0x04 register with address=%d, baudrate = %d", this->current_address_, this->current_baudrate_);
	}
    else{
	  ESP_LOGD(TAG, "JSY1039: Read bad values from 0x04 : address=%d, baudrate = %d, keep current address =%d, baudrate %d", data[0], data[1] , this->current_address_ , this->current_baudrate_);
    }	
	this->read_data_ = 1;
  }
  else if(this->read_data_ == 3){ // write 0x04 register
   	if ( data.size() < 2){
	  ESP_LOGD(TAG, "JSY1039: failed to write new values 0x04 register with address=%d, baudrate = %d", this->new_address_, this->new_baudrate_);
	}
    else{
	  ESP_LOGD(TAG, "JSY1039: wrote new values into 0x04 register with address=%d, baudrate = %d", this->new_address_ , this->new_baudrate_);
    }	
	this->read_data_ = 1; 
  }
  else if(this->read_data_ == 4){ // Reset Energy
   	if ( data.size() < 2){
	  ESP_LOGD(TAG, "JSY1039: failed to reset Energy");
	}
    else{
	  ESP_LOGD(TAG, "JSY1039: successfully reset Energy");
    }	
	this->read_data_ = 1; 
  }
}

void JSY1039::update() { this->send(JSY1039_CMD_READ_IN_REGISTERS, JSY1039_REGISTER_DATA_START , JSY1039_REGISTER_DATA_COUNT); }

void JSY1039::dump_config() {
  ESP_LOGCONFIG(TAG, "JSY1039:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Power", this->power_sensor_);
  LOG_SENSOR("", "Positive Energy", this->pos_energy_sensor_);
  LOG_SENSOR("", "Negative Energy", this->neg_energy_sensor_);
  LOG_SENSOR("", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("", "Power Factor", this->power_factor_sensor_);
}

void JSY1039::read_register04() {
  this->read_data_ = 2;
  std::vector<uint8_t> cmd;
  cmd.push_back(this->address_); 
  cmd.push_back(JSY1039_CMD_READ_IN_REGISTERS);
  cmd.push_back(0x00);  
  cmd.push_back(JSY1039_REGISTER_SETTINGS_START);
  cmd.push_back(0x00);
  cmd.push_back(JSY1039_REGISTER_SETTINGS_COUNT);
  ESP_LOGD(TAG, "JSY1039: reading values from 0x04 register"); 
  this->send_raw(cmd);
}

void JSY1039::write_register04(uint8_t new_address , uint8_t new_baudrate) {
  if ((new_address>=1) & (new_address <= 255) & (new_baudrate>=5) & (new_baudrate <= 8)){
    this->read_data_ = 3;
	std::vector<uint8_t> cmd;
    cmd.push_back(0x00);  // broadcast address
    cmd.push_back(JSY1039_CMD_WRITE_IN_REGISTERS);
    cmd.push_back(0x00);  
    cmd.push_back(JSY1039_REGISTER_SETTINGS_START);
    cmd.push_back(0x00);
    cmd.push_back(0x01); 
    cmd.push_back(0x02);
    cmd.push_back(new_address);
    cmd.push_back(new_baudrate);
    ESP_LOGD(TAG, "JSY1039: writing values into 0x04 register: address=%d, baudrate = %d", new_address_, new_baudrate); 
    this->send_raw(cmd);
  } 
  else{
	 ESP_LOGD(TAG, "JSY1039: attempt to write bad values into 0x04 : Address=%d, baudrate = %d", new_address_, new_baudrate); 
  }
}

void JSY1039::reset_energy() {
  this->read_data_ = 4;	
  std::vector<uint8_t> cmd;
  cmd.push_back(this->address_);
  cmd.push_back(JSY1039_CMD_WRITE_IN_REGISTERS);
  cmd.push_back(0x01);  
  cmd.push_back(JSY1039_RESET_ENERGY_LB);
  cmd.push_back(0x00);
  cmd.push_back(0x04); 
  cmd.push_back(0x08);
  
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  cmd.push_back(0x00);
  ESP_LOGD(TAG, "JSY1039: sending reset Energy command"); 
  this->send_raw(cmd);
}


}  // namespace jsy1039
}  // namespace esphome
