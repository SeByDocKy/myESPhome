#include "jsy194.h"
#include "esphome/core/log.h"

namespace esphome {
namespace jsy194 {

static const char *const TAG = "jsy194";
static const uint8_t JSY194_CMD_READ_IN_REGISTERS = 0x03;   // multiple registers
static const uint8_t JSY194_CMD_WRITE_IN_REGISTERS = 0x10;
static const uint8_t JSY194_REGISTER_SETTINGS_START = 0x04; // modbus address & databaud
static const uint8_t JSY194_REGISTER_SETTINGS_COUNT = 0x01;  // 1 x 16-bit setting registers
static const uint8_t JSY194_RESET_RESET_POS_ENERGY1_LB = 0x4B; // 0x004B;
static const uint8_t JSY194_RESET_RESET_NEG_ENERGY1_LB = 0x4D; // 0x004D;
static const uint8_t JSY194_RESET_RESET_POS_ENERGY2_LB = 0x53; // 0x0053;
static const uint8_t JSY194_RESET_RESET_NEG_ENERGY2_LB = 0x55; // 0x0055;
static const uint16_t JSY194_REGISTER_DATA_START = 0x0048;
static const uint8_t JSY194_REGISTER_DATA_COUNT = 14;  // 14 x 32-bit data registers

void JSY194::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up JSY194..."); 
}

void JSY194::on_modbus_data(const std::vector<uint8_t> &data) {
  if ((this->read_data_ == 1) & (data.size() < JSY194_REGISTER_DATA_COUNT*4)) {
    ESP_LOGW(TAG, "Invalid size for JSY194 data!");
    return;
  }

  auto jsy194_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 0]) << 8) | (uint16_t(data[i + 1]) << 0);
  };
  auto jsy194_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(jsy194_get_16bit(i + 0)) << 16) | (uint32_t(jsy194_get_16bit(i + 2)) << 0);
  };
 
  if (this->read_data_ == 1){
	
    float sign1 = float(data[24] & 0b00000001);  
    float voltage1 = static_cast<float>(jsy194_get_32bit(0))/10000.0f;  // max 429496.7295 V
    float raw_current = static_cast<float>(jsy194_get_32bit(4)); 
    float current1 = ((1.0f - sign1)*raw_current - sign1*raw_current)/10000.0f;  // min -429496.7295 A, max 429496.7295 A 
    float raw_power   = static_cast<float>(jsy194_get_32bit(8));
    float power1 = ((1.0f - sign1)*raw_power - sign1*raw_power)/10000.0f;  // min -429496.7295 W, max 429496.7295 W  
    float pos_energy1 = static_cast<float>(jsy194_get_32bit(12))/10000.0f; // max 429496.7295 kWh
    float power_factor1 = static_cast<float>(jsy194_get_32bit(16))/1000.0f;   // max 4294967.295
    float neg_energy1 = static_cast<float>(jsy194_get_32bit(20))/10000.0f; // max 42 949 673 kWh
    float frequency1 = static_cast<float>(jsy194_get_32bit(28))/100.0f;  // max 655.35 Hz
	
    float sign2 = float(data[25] & 0b00000001);
    float voltage2 = static_cast<float>(jsy194_get_32bit(32))/10000.0f;  // max 429496.7295 V
    raw_current = static_cast<float>(jsy194_get_32bit(36));  
    float current2 = ( (1.0f - sign2)*raw_current - sign2*raw_current)/10000.0f;  // min -429496.7295 A, max 429496.7295 A
    raw_power   = static_cast<float>(jsy194_get_32bit(40));
    float power2 = ((1.0f - sign2)*raw_power - sign2*raw_power)/10000.0f;  // min -429496.7295 W, max 429496.7295 W
    float pos_energy2 = static_cast<float>(jsy194_get_32bit(44))/10000.0f; // max 429496.7295 kWh
    float power_factor2 = static_cast<float>(jsy194_get_32bit(48))/1000.0f;   // max 4294967.295
    float neg_energy2 = static_cast<float>(jsy194_get_32bit(52))/10000.0f; // max 42 949 673 kWh  
    float frequency2 = frequency1;  // max 655.35 Hz
    ESP_LOGVV(TAG, "V1=%.1f V, I1=%.3f A, P1=%.1f W, E1+=%.1f kWh, E1-=%.1f kWh, F1=%.1f Hz, PF1=%.2f , V2=%.1f V, I2=%.3f A, P2=%.1f W, E2+=%.1f kWh, E2-=%.1f kWh, F2=%.1f Hz, PF2=%.2f", voltage1, current1, power1,
             pos_energy1, neg_energy1, frequency1, power_factor1, voltage2, current2, power2, pos_energy2, neg_energy2, frequency2, power_factor2);

    if (this->voltage1_sensor_ != nullptr)
      this->voltage1_sensor_->publish_state(voltage1);
    if (this->current1_sensor_ != nullptr)
      this->current1_sensor_->publish_state(current1);
    if (this->power1_sensor_ != nullptr)
      this->power1_sensor_->publish_state(power1);
    if (this->pos_energy1_sensor_ != nullptr)
      this->pos_energy1_sensor_->publish_state(pos_energy1);
    if (this->neg_energy1_sensor_ != nullptr)
      this->neg_energy1_sensor_->publish_state(neg_energy1);
    if (this->frequency1_sensor_ != nullptr)
      this->frequency1_sensor_->publish_state(frequency1);
    if (this->power_factor1_sensor_ != nullptr)
      this->power_factor1_sensor_->publish_state(power_factor1);
    if (this->voltage2_sensor_ != nullptr)
      this->voltage2_sensor_->publish_state(voltage2);
    if (this->current2_sensor_ != nullptr)
      this->current2_sensor_->publish_state(current2);
    if (this->power2_sensor_ != nullptr)
      this->power2_sensor_->publish_state(power2);
    if (this->pos_energy2_sensor_ != nullptr)
      this->pos_energy2_sensor_->publish_state(pos_energy2);
    if (this->neg_energy2_sensor_ != nullptr)
      this->neg_energy2_sensor_->publish_state(neg_energy2);
    if (this->frequency2_sensor_ != nullptr)
      this->frequency2_sensor_->publish_state(frequency2);
    if (this->power_factor2_sensor_ != nullptr)
      this->power_factor2_sensor_->publish_state(power_factor2);  
	
  }
  else if(this->read_data_ == 2){ // read 0x04 register
   	if ( (data[0]>=1) & (data[0] <= 255) & (data[1]>=3) & (data[0] <= 8)){
	  this->current_address_ = data[0];
	  this->current_baudrate_= data[1];
	  ESP_LOGD(TAG, "JSY194: Read 0x04 register with address=%d, baudrate = %d", this->current_address_, this->current_baudrate_);
	}
    else{
	  ESP_LOGD(TAG, "JSY194: Read bad values from 0x04 : address=%d, baudrate = %d, keep current address =%d, baudrate %d", data[0], data[1] , this->current_address_ , this->current_baudrate_);
    }	
	this->read_data_ = 1; 
  }
  else if(this->read_data_ == 3){ // write 0x04 register
   	if ( data.size() < 2){
	  ESP_LOGD(TAG, "JSY194: failed to write new values 0x04 register with address=%d, baudrate = %d", this->new_address_, this->new_baudrate_);
	}
    else{
	  ESP_LOGD(TAG, "JSY194: wrote new values into 0x04 register with address=%d, baudrate = %d", this->new_address_ , this->new_baudrate_);
    }	
	this->read_data_ = 1; 
  }
  else if(this->read_data_ == 4){ // Reset Energy1Pos
   	if ( data.size() < 2){
	  ESP_LOGD(TAG, "JSY194: failed to reset Energy1Pos");
	}
    else{
	  ESP_LOGD(TAG, "JSY194: successfully reset Energy1Pos");
    }	
	this->read_data_ = 1; 
  }
  else if(this->read_data_ == 5){ // Reset Energy1Neg
    if ( data.size() < 2){
	  ESP_LOGD(TAG, "JSY194: failed to reset Energy1Neg");
	}
    else{
	  ESP_LOGD(TAG, "JSY194: successfully reset Energy1Neg");
    }	
	this->read_data_ = 1; 
  }
  else if(this->read_data_ == 6){ // Reset Energy2Pos
   	if ( data.size() < 2){
	  ESP_LOGD(TAG, "JSY194: failed to reset Energy2Pos");
	}
    else{
	  ESP_LOGD(TAG, "JSY194: successfully reset Energy2Pos");
    }	
	this->read_data_ = 1; 
  }
  else if(this->read_data_ == 7){ // Reset Energy2Neg
    if ( data.size() < 2){
	  ESP_LOGD(TAG, "JSY194: failed to reset Energy2Neg");
	}
    else{
	  ESP_LOGD(TAG, "JSY194: successfully reset Energy2Neg");
    }	
	this->read_data_ = 1; 
  }  
}

void JSY194::update() { 
  this->send(JSY194_CMD_READ_IN_REGISTERS, JSY194_REGISTER_DATA_START , JSY194_REGISTER_DATA_COUNT*1); 
}

void JSY194::dump_config() {
  ESP_LOGCONFIG(TAG, "JSY194:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  LOG_SENSOR("", "Voltage1", this->voltage1_sensor_);
  LOG_SENSOR("", "Current1", this->current1_sensor_);
  LOG_SENSOR("", "Power1", this->power1_sensor_);
  LOG_SENSOR("", "Positive Energy1", this->pos_energy1_sensor_);
  LOG_SENSOR("", "Negative Energy1", this->neg_energy1_sensor_);
  LOG_SENSOR("", "Frequency1", this->frequency1_sensor_);
  LOG_SENSOR("", "Power Factor1", this->power_factor1_sensor_);
  
  LOG_SENSOR("", "Voltage2", this->voltage2_sensor_);
  LOG_SENSOR("", "Current2", this->current2_sensor_);
  LOG_SENSOR("", "Power2", this->power2_sensor_);
  LOG_SENSOR("", "Positive Energy2", this->pos_energy2_sensor_);
  LOG_SENSOR("", "Negative Energy2", this->neg_energy2_sensor_);
  LOG_SENSOR("", "Frequency2", this->frequency2_sensor_);
  LOG_SENSOR("", "Power Factor2", this->power_factor2_sensor_);
}

void JSY194::read_register04() {
  this->read_data_ = 2;
  std::vector<uint8_t> cmd;
  cmd.push_back(this->address_); 
  cmd.push_back(JSY194_CMD_READ_IN_REGISTERS);
  cmd.push_back(0x00);  
  cmd.push_back(JSY194_REGISTER_SETTINGS_START);
  cmd.push_back(0x00);
  cmd.push_back(JSY194_REGISTER_SETTINGS_COUNT);
  ESP_LOGD(TAG, "JSY194: reading values from 0x04 register"); 
  this->send_raw(cmd);
}

void JSY194::write_register04(uint8_t new_address , uint8_t new_baudrate) {
  if ((new_address>=1) & (new_address <= 255) & (new_baudrate>=3) & (new_baudrate <= 8)){
    this->read_data_ = 3;
	std::vector<uint8_t> cmd;
    cmd.push_back(0x00);  // broadcast address
    cmd.push_back(JSY194_CMD_WRITE_IN_REGISTERS);
    cmd.push_back(0x00);  
    cmd.push_back(JSY194_REGISTER_SETTINGS_START);
    cmd.push_back(0x00);
    cmd.push_back(0x01); 
    cmd.push_back(0x02);
    cmd.push_back(new_address);
    cmd.push_back(new_baudrate);
    ESP_LOGD(TAG, "JSY194: writing values into 0x04 register: address=%d, baudrate = %d", new_address_, new_baudrate); 
    this->send_raw(cmd);
  } 
  else{
	 ESP_LOGD(TAG, "JSY194: attempt to write bad values into 0x04 : Address=%d, baudrate = %d", new_address_, new_baudrate); 
  }
}

void JSY194::reset_energy1pos() {
  this->read_data_ = 4;
  std::vector<uint8_t> cmdpos;
  cmdpos.push_back(this->address_);
  cmdpos.push_back(JSY194_CMD_WRITE_IN_REGISTERS);
  cmdpos.push_back(0x00);  
  cmdpos.push_back(JSY194_RESET_RESET_POS_ENERGY1_LB);
  cmdpos.push_back(0x00);
  cmdpos.push_back(0x02); 
  cmdpos.push_back(0x04);
  
  cmdpos.push_back(0x00);
  cmdpos.push_back(0x00);
  cmdpos.push_back(0x00);
  cmdpos.push_back(0x00);
  ESP_LOGD(TAG, "JSY194: sending reset Energy1Pos command"); 
  this->send_raw(cmdpos);
}  
void JSY194::reset_energy1neg() {
  this->read_data_ = 5;
  std::vector<uint8_t> cmdneg;
  cmdneg.push_back(this->address_);
  cmdneg.push_back(JSY194_CMD_WRITE_IN_REGISTERS);
  cmdneg.push_back(0x00);  
  cmdneg.push_back(JSY194_RESET_RESET_NEG_ENERGY1_LB);
  cmdneg.push_back(0x00);
  cmdneg.push_back(0x02); 
  cmdneg.push_back(0x04);
  
  cmdneg.push_back(0x00);
  cmdneg.push_back(0x00);
  cmdneg.push_back(0x00);
  cmdneg.push_back(0x00);  
  ESP_LOGD(TAG, "JSY194: sending reset Energy1Neg command"); 
  this->send_raw(cmdneg);
}
void JSY194::reset_energy2pos() {
  this->read_data_ = 6;
  std::vector<uint8_t> cmdpos;
  cmdpos.push_back(this->address_);
  cmdpos.push_back(JSY194_CMD_WRITE_IN_REGISTERS);
  cmdpos.push_back(0x00);  
  cmdpos.push_back(JSY194_RESET_RESET_POS_ENERGY2_LB);
  cmdpos.push_back(0x00);
  cmdpos.push_back(0x02); 
  cmdpos.push_back(0x04);
  
  cmdpos.push_back(0x00);
  cmdpos.push_back(0x00);
  cmdpos.push_back(0x00);
  cmdpos.push_back(0x00);
  ESP_LOGD(TAG, "JSY194: sending reset Energy2Pos command"); 
  this->send_raw(cmdpos);
} 
void JSY194::reset_energy2neg() {
  this->read_data_ = 7;
  std::vector<uint8_t> cmdneg;
  cmdneg.push_back(this->address_);
  cmdneg.push_back(JSY194_CMD_WRITE_IN_REGISTERS);
  cmdneg.push_back(0x00);  
  cmdneg.push_back(JSY194_RESET_RESET_NEG_ENERGY2_LB);
  cmdneg.push_back(0x00);
  cmdneg.push_back(0x02); 
  cmdneg.push_back(0x04);
  
  cmdneg.push_back(0x00);
  cmdneg.push_back(0x00);
  cmdneg.push_back(0x00);
  cmdneg.push_back(0x00);  
  ESP_LOGD(TAG, "JSY194: sending reset Energy2Neg command"); 
  this->send_raw(cmdneg);  
}

}  // namespace jsy194
}  // namespace esphome
