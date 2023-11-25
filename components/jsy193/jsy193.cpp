#include "jsy193.h"
#include "esphome/core/log.h"

namespace esphome {
namespace jsy193 {

static const char *const TAG = "jsy193";

static const uint8_t JSY193_CMD_READ_IN_REGISTERS = 0x03;   // multiple registers
static const uint8_t JSY193_CMD_WRITE_IN_REGISTERS = 0x10;
static const uint16_t JSY193_REGISTER_SETTINGS_START = 0x0004;
static const uint16_t JSY193_REGISTER_DATA_START = 0x0100;
static const uint8_t JSY193_RESET_RESET_ENERGY1_LB = 0x04; // 0x0104;
static const uint8_t JSY193_RESET_RESET_ENERGY2_LB = 0x0E; // 0x010E;
static const uint8_t JSY193_REGISTER_COUNT = 20;  // 20x 16-bit registers

void JSY193::setup() { 
  ESP_LOGCONFIG(TAG, "Setting up JSY193...");
  this->read_data_ = false;
  // this->send(JSY193_CMD_READ_IN_REGISTERS, JSY193_REGISTER_SETTINGS_START , 1);
  std::vector<uint8_t> cmd;
  cmd.push_back(0x00); // this->address_
  cmd.push_back(JSY193_CMD_READ_IN_REGISTERS);
  cmd.push_back(0x00);  
  cmd.push_back(0x04);
  cmd.push_back(0x01);
  this->send_raw(cmd);
}

void JSY193::on_modbus_data(const std::vector<uint8_t> &data) {
  if ((data.size() < JSY193_REGISTER_COUNT*2)) {   // (this->read_data_ == true) | 
    ESP_LOGW(TAG, "Invalid size for JSY193 data!");
    return;
  }

  auto jsy193_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 0]) << 8) | (uint16_t(data[i + 1]) << 0);
  };
  auto jsy193_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(jsy193_get_16bit(i + 0)) << 16) | (uint32_t(jsy193_get_16bit(i + 2)) << 0);
  };
// /*  
  if (this->read_data_ == false){
	this->current_address_ = data[0];  
	this->current_baudrate_= data[1];
	ESP_LOGD(TAG, "JSY193: Address=%d, baudrate = %d", this->current_address_, this->current_baudrate_);
	this->read_data_ = true;
  }
  else{
// */	  
    uint16_t raw_voltage = jsy193_get_16bit(0);
    float voltage1 = raw_voltage / 100.0f;  // max 655.35 V

    uint16_t raw_sign = jsy193_get_16bit(6); //0 for positive, 1 for negative 
  
    uint16_t raw_current = jsy193_get_16bit(2);  
    float current1 = ((1 - raw_sign)*raw_current - raw_sign*raw_current)/100.0f;  // min -655.35 A, max 655.35 A
  
    uint16_t raw_power   = jsy193_get_16bit(4);
    float power1 = (1 - raw_sign)*raw_power - raw_sign*raw_power;  // min 65535 W max 65535 W
    
    float pos_energy1 = static_cast<float>(jsy193_get_32bit(8))/100.0f; // max 42 949 673 kWh
    float neg_energy1 = static_cast<float>(jsy193_get_32bit(12))/100.0f; // max 42 949 673 kWh

    uint16_t raw_power_factor = jsy193_get_16bit(16);
    float power_factor1 = raw_power_factor / 1000.0f;  // max 65.535

    uint16_t raw_frequency = jsy193_get_16bit(18);
    float frequency1 = raw_frequency / 100.0f;  // max 655.35 Hz
  
    raw_voltage = jsy193_get_16bit(20);
    float voltage2 = raw_voltage / 100.0f;  // max 655.35 V

    raw_sign = jsy193_get_16bit(26); //0 for positive, 1 for negative 
  
    raw_current = jsy193_get_16bit(22);  
    float current2 = ((1 - raw_sign)*raw_current - raw_sign*raw_current)/100.0f;  // min -655.35 A, max 655.35 A 
  
    raw_power   = jsy193_get_16bit(24);
    float power2 = (1 - raw_sign)*raw_power - raw_sign*raw_power;  // min 65535 W max 65535 W
    
    float pos_energy2 = static_cast<float>(jsy193_get_32bit(28))/100.0f; // max 42 949 673 kWh
    float neg_energy2 = static_cast<float>(jsy193_get_32bit(32))/100.0f; // max 42 949 673 kWh

    raw_power_factor = jsy193_get_16bit(36);
    float power_factor2 = raw_power_factor / 1000.0f;  // max 65.535

    raw_frequency = jsy193_get_16bit(38);
    float frequency2 = raw_frequency / 100.0f;     // max 655.35 Hz

  
    ESP_LOGD(TAG, "JSY193: addr = %d, baudrate=%d, V1=%.1f V, I1=%.3f A, P1=%.1f W, E1+=%.1f kWh , E1-=%.1f kWh, F1=%.1f Hz, PF1=%.2f , V2=%.1f V, I2=%.3f A, P2=%.1f W, E2+=%.1f kWh , E2-=%.1f kWh, F2=%.1f Hz, PF2=%.2f", this->current_address_, this->current_baudrate_, voltage1, current1, power1,
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
// /*  
  }
// */  
}

void JSY193::update() { this->send(JSY193_CMD_READ_IN_REGISTERS, JSY193_REGISTER_DATA_START , JSY193_REGISTER_COUNT); }

void JSY193::dump_config() {
  ESP_LOGCONFIG(TAG, "JSY193:");
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

void JSY193::change_modbus_address_(uint8_t new_address) {
  std::vector<uint8_t> cmd;
  cmd.push_back(0x00);  // broadcast address
  cmd.push_back(JSY193_CMD_WRITE_IN_REGISTERS);
  cmd.push_back(0x00);  
  cmd.push_back(0x04);
  cmd.push_back(0x00);
  cmd.push_back(0x01); 
  cmd.push_back(0x02);
  
  cmd.push_back(new_address);
  cmd.push_back(0x06);  // this->current_baudrate_
  
  this->send_raw(cmd);
  
}

void JSY193::change_modbus_baudrate_(uint8_t new_baudrate) {
  std::vector<uint8_t> cmd;
  cmd.push_back(0x00);  // broadcast address
  cmd.push_back(JSY193_CMD_WRITE_IN_REGISTERS);
  cmd.push_back(0x00);  
  cmd.push_back(0x04);
  cmd.push_back(0x00);
  cmd.push_back(0x01); 
  cmd.push_back(0x02);
  
  cmd.push_back(0x01); // this->current_address_
  cmd.push_back(new_baudrate);  
  
  this->send_raw(cmd);
}

void JSY193::reset_energy1_() {
  std::vector<uint8_t> cmd;
  cmd.push_back(this->address_);
  cmd.push_back(JSY193_CMD_WRITE_IN_REGISTERS);
  cmd.push_back(0x01);  
  cmd.push_back(JSY193_RESET_RESET_ENERGY1_LB);
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
     
  this->send_raw(cmd);
}
void JSY193::reset_energy2_() {
  std::vector<uint8_t> cmd;
  cmd.push_back(this->address_);
  cmd.push_back(JSY193_CMD_WRITE_IN_REGISTERS);
  cmd.push_back(0x01);
  cmd.push_back(JSY193_RESET_RESET_ENERGY2_LB);
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
    
  this->send_raw(cmd);
}

}  // namespace jsy193
}  // namespace esphome
